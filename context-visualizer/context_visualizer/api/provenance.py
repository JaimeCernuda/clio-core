"""Provenance API endpoints with SSE for live context graph updates."""

import json
import time

from flask import Blueprint, Response, jsonify, request

from .. import chimaera_client
from ..analysis.context_graph import compute_analysis

bp = Blueprint("provenance", __name__)


def _flatten_results(raw):
    """Extract data from container-keyed monitor results."""
    for _cid, data in raw.items():
        if isinstance(data, list):
            return data
        if isinstance(data, dict):
            return data
        if isinstance(data, str):
            try:
                return json.loads(data)
            except (json.JSONDecodeError, TypeError):
                return data
    return []


def _flatten_and_parse(raw):
    """Flatten monitor results and parse any JSON-string items."""
    items = _flatten_results(raw)
    if not isinstance(items, list):
        items = [items] if items else []
    parsed = []
    for item in items:
        if isinstance(item, str):
            try:
                parsed.append(json.loads(item))
            except (json.JSONDecodeError, TypeError):
                parsed.append(item)
        else:
            parsed.append(item)
    return parsed


@bp.route("/sessions")
def list_sessions():
    """List all tracked agent sessions."""
    try:
        raw = chimaera_client.get_sessions()
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503

    sessions = _flatten_results(raw)
    return jsonify({"sessions": sessions})


@bp.route("/session/<session_id>/interactions")
def get_session_interactions(session_id):
    """Get all interaction records for a session."""
    try:
        raw = chimaera_client.get_session_interactions(session_id)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503

    interactions = _flatten_results(raw)
    # Parse JSON strings if needed
    parsed = []
    for item in interactions:
        if isinstance(item, str):
            try:
                parsed.append(json.loads(item))
            except (json.JSONDecodeError, TypeError):
                parsed.append(item)
        else:
            parsed.append(item)

    return jsonify({"interactions": parsed})


@bp.route("/session/<session_id>/graph")
def get_context_graph(session_id):
    """Get all context graph diff nodes for a session."""
    since = request.args.get("since", 0, type=int)
    try:
        raw = chimaera_client.get_context_graph(session_id, since=since)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503

    nodes = _flatten_results(raw)
    parsed = []
    for item in nodes:
        if isinstance(item, str):
            try:
                parsed.append(json.loads(item))
            except (json.JSONDecodeError, TypeError):
                parsed.append(item)
        else:
            parsed.append(item)

    return jsonify({"nodes": parsed})


@bp.route("/session/<session_id>/graph/<int:seq_id>")
def get_context_node(session_id, seq_id):
    """Get a single context graph diff node."""
    try:
        raw = chimaera_client.get_context_node(session_id, seq_id)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503

    node = _flatten_results(raw)
    if isinstance(node, str):
        try:
            node = json.loads(node)
        except (json.JSONDecodeError, TypeError):
            pass

    return jsonify({"node": node})


@bp.route("/session/<session_id>/stream")
def stream_context_graph(session_id):
    """SSE endpoint for live context graph updates."""
    def event_stream():
        last_seq = 0
        while True:
            try:
                raw = chimaera_client.get_context_graph(
                    session_id, since=last_seq)
                nodes = _flatten_results(raw)
                for item in nodes:
                    if isinstance(item, str):
                        try:
                            node = json.loads(item)
                        except (json.JSONDecodeError, TypeError):
                            continue
                    else:
                        node = item

                    yield f"data: {json.dumps(node)}\n\n"
                    seq = node.get("sequence_id", 0)
                    if seq > last_seq:
                        last_seq = seq
            except Exception:
                pass
            time.sleep(1)

    return Response(
        event_stream(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",
        },
    )


@bp.route("/session/<session_id>/analysis")
def get_session_analysis(session_id):
    """Compute rich analysis data for all visualization charts.

    Accepts an optional ?include=id1,id2,... query param to merge additional
    sessions (e.g. child/subagent sessions) into the analysis.
    """
    extra = [s for s in request.args.get("include", "").split(",") if s]
    all_ids = [session_id] + extra

    all_interactions = []
    all_nodes = []
    for sid in all_ids:
        try:
            raw_i = chimaera_client.get_session_interactions(sid)
            raw_n = chimaera_client.get_context_graph(sid)
        except Exception as exc:
            if sid == session_id:
                return jsonify({"error": str(exc)}), 503
            continue
        all_interactions.extend(_flatten_and_parse(raw_i))
        all_nodes.extend(_flatten_and_parse(raw_n))

    return jsonify(compute_analysis(all_interactions, all_nodes))
