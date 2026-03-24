"""Flask blueprint: checkpoint management API."""

import json
from flask import Blueprint, Response, request

from ..checkpointing.checkpoint_manager import manager, CheckpointType

bp = Blueprint("checkpoints", __name__)


@bp.route("/session/<sid>/checkpoints", methods=["GET"])
def list_checkpoints(sid):
    """List all checkpoints for a session."""
    checkpoints = manager.get_checkpoints(sid)
    return Response(
        json.dumps({
            "session_id": sid,
            "checkpoints": [cp.to_dict() for cp in checkpoints],
        }),
        content_type="application/json",
    )


@bp.route("/session/<sid>/checkpoints", methods=["POST"])
def create_checkpoint(sid):
    """Create a manual checkpoint."""
    data = request.get_json(silent=True) or {}
    sequence_id = data.get("sequence_id")
    name = data.get("name", "manual")
    metadata = data.get("metadata", {})

    if sequence_id is None:
        return Response(
            json.dumps({"error": "sequence_id required"}),
            status=400,
            content_type="application/json",
        )

    cp = manager.create_checkpoint(
        sid,
        sequence_id,
        name,
        checkpoint_type=CheckpointType.MANUAL,
        metadata=metadata,
    )

    return Response(
        json.dumps(cp.to_dict()),
        status=201,
        content_type="application/json",
    )


@bp.route("/session/<sid>/checkpoints/best", methods=["GET"])
def find_best(sid):
    """Find best restore point before a given sequence_id."""
    before = request.args.get("before", type=int)
    if before is None:
        return Response(
            json.dumps({"error": "before query param required"}),
            status=400,
            content_type="application/json",
        )

    cp = manager.find_best_restore_point(sid, before)
    return Response(
        json.dumps({
            "session_id": sid,
            "checkpoint": cp.to_dict() if cp else None,
        }),
        content_type="application/json",
    )


@bp.route("/session/<sid>/checkpoints/<int:seq>/score", methods=["GET"])
def score_restore(sid, seq):
    """Score a specific restore point."""
    token_count = request.args.get("token_count", default=0, type=int)
    latency_ms = request.args.get("latency_ms", default=0.0, type=float)

    score = manager.score_restore_point(sid, seq, token_count, latency_ms)
    return Response(
        json.dumps({
            "session_id": sid,
            "sequence_id": seq,
            "score": score,
        }),
        content_type="application/json",
    )


@bp.route("/session/<sid>/rollback/plan", methods=["POST"])
def build_plan(sid):
    """Build a rollback plan."""
    data = request.get_json(silent=True) or {}
    trigger_sequence_id = data.get("trigger_sequence_id")
    error_description = data.get("error_description", "")

    if trigger_sequence_id is None:
        return Response(
            json.dumps({"error": "trigger_sequence_id required"}),
            status=400,
            content_type="application/json",
        )

    plan = manager.build_rollback_plan(sid, trigger_sequence_id, error_description)
    return Response(
        json.dumps(plan.to_dict()),
        content_type="application/json",
    )


@bp.route("/session/<sid>/rollback/execute", methods=["POST"])
def execute_rollback(sid):
    """Execute a rollback by registering a replay."""
    data = request.get_json(silent=True) or {}
    plan_id = data.get("plan_id")

    plan = None
    if plan_id:
        # Look up in recent plans
        plan = manager._recent_plans.get(plan_id)
    else:
        # Build inline
        trigger_sequence_id = data.get("trigger_sequence_id")
        error_description = data.get("error_description", "")

        if trigger_sequence_id is None:
            return Response(
                json.dumps({"error": "plan_id or trigger_sequence_id required"}),
                status=400,
                content_type="application/json",
            )

        plan = manager.build_rollback_plan(sid, trigger_sequence_id, error_description)

    if plan is None:
        return Response(
            json.dumps({"error": "plan not found"}),
            status=404,
            content_type="application/json",
        )

    manager.register_replay(sid, plan)
    return Response(
        json.dumps({
            "registered": True,
            "session_id": sid,
            "plan": plan.to_dict(),
        }),
        content_type="application/json",
    )


@bp.route("/checkpoints", methods=["GET"])
def all_checkpoints():
    """Get all checkpoints across all sessions."""
    result = {}
    for session_id in sorted(manager._checkpoints.keys()):
        result[session_id] = [cp.to_dict() for cp in manager.get_checkpoints(session_id)]
    return Response(
        json.dumps({"sessions": result}),
        content_type="application/json",
    )
