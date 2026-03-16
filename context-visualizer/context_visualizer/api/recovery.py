"""Flask blueprint: recovery event and LangGraph checkpoint API."""

import json

from flask import Blueprint, Response, request

from .. import chimaera_client

bp = Blueprint("recovery", __name__)


@bp.route("/session/<sid>/recovery_events", methods=["GET"])
def list_recovery_events(sid):
    """List all recovery events for a session."""
    try:
        events = chimaera_client.get_recovery_events(sid)
    except Exception as exc:
        return Response(
            json.dumps({"error": str(exc)}),
            status=502,
            content_type="application/json",
        )
    return Response(
        json.dumps({"session_id": sid, "events": events}),
        content_type="application/json",
    )


@bp.route("/session/<sid>/recovery_events/<blob>/ack", methods=["POST"])
def ack_recovery_event(sid, blob):
    """Acknowledge a recovery event."""
    try:
        chimaera_client.ack_recovery_event(sid, blob)
    except Exception as exc:
        return Response(
            json.dumps({"error": str(exc)}),
            status=502,
            content_type="application/json",
        )
    return Response(
        json.dumps({"acknowledged": True, "blob_name": blob}),
        content_type="application/json",
    )


@bp.route("/checkpoint/<sid>/<int:seq_id>", methods=["GET"])
def get_checkpoint_messages(sid, seq_id):
    """Return the messages array for a specific interaction (checkpoint view)."""
    try:
        result = chimaera_client.get_interaction(sid, seq_id)
        interaction = None
        for _, data in result.items():
            if isinstance(data, str):
                try:
                    interaction = json.loads(data)
                except Exception:
                    pass
            elif isinstance(data, dict):
                interaction = data
            break
        if interaction is None:
            return Response(
                json.dumps({"error": "interaction not found"}),
                status=404,
                content_type="application/json",
            )
        messages = (
            interaction.get("request", {})
            .get("body", {})
            .get("messages", [])
        )
        return Response(
            json.dumps({"session_id": sid, "seq_id": seq_id, "messages": messages}),
            content_type="application/json",
        )
    except Exception as exc:
        return Response(
            json.dumps({"error": str(exc)}),
            status=502,
            content_type="application/json",
        )


@bp.route("/recovery/graph", methods=["GET"])
def recovery_graph():
    """Return a directed graph of cross-agent recovery events.

    Returns {nodes: [{id, label, event_count}], edges: [{source, target, count, events}]}.
    """
    try:
        sessions_result = chimaera_client.get_sessions()
        session_ids = []
        for _, sessions in sessions_result.items():
            if isinstance(sessions, list):
                for s in sessions:
                    if isinstance(s, dict):
                        sid = s.get("session_id", "")
                        if sid:
                            session_ids.append(sid)

        nodes_map: dict = {}
        edges_map: dict = {}

        for sid in session_ids:
            try:
                events = chimaera_client.get_recovery_events(sid)
            except Exception:
                events = []

            if events:
                nodes_map[sid] = nodes_map.get(sid, 0) + len(events)

            for ev in events:
                src = ev.get("source_session_id", "")
                tgt = ev.get("target_session_id", "")
                if src and tgt and src != tgt:
                    if src not in nodes_map:
                        nodes_map[src] = 0
                    if tgt not in nodes_map:
                        nodes_map[tgt] = 0
                    key = f"{src}→{tgt}"
                    if key not in edges_map:
                        edges_map[key] = {"source": src, "target": tgt, "count": 0, "events": []}
                    edges_map[key]["count"] += 1
                    edges_map[key]["events"].append({
                        "event_id": ev.get("event_id", ""),
                        "event_type": ev.get("event_type", ""),
                        "timestamp": ev.get("timestamp", ""),
                        "acknowledged": ev.get("acknowledged", False),
                    })

        nodes = [{"id": sid, "label": sid, "event_count": cnt} for sid, cnt in nodes_map.items()]
        edges = list(edges_map.values())

        return Response(
            json.dumps({"nodes": nodes, "edges": edges}),
            content_type="application/json",
        )
    except Exception as exc:
        return Response(
            json.dumps({"error": str(exc)}),
            status=502,
            content_type="application/json",
        )
