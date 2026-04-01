"""Overhead stats API: toggle logging, query aggregates, dump to file."""

import datetime
import json
import os

from flask import Blueprint, jsonify, request

from .. import chimaera_client

bp = Blueprint("overhead", __name__)


def _flatten(raw):
    """Extract the first non-empty decoded value from a Monitor result dict."""
    for _, v in raw.items():
        if isinstance(v, dict):
            return v
    return {}


@bp.route("/overhead/stats")
def get_overhead_stats():
    """Return aggregate overhead stats from all three CEE components."""
    stats = {}

    try:
        raw = chimaera_client.get_proxy_dispatch_stats()
        stats["proxy"] = _flatten(raw)
    except Exception as exc:
        stats["proxy"] = {"error": str(exc)}

    try:
        raw = chimaera_client.get_tracker_overhead_stats()
        stats["tracker"] = _flatten(raw)
    except Exception as exc:
        stats["tracker"] = {"error": str(exc)}

    try:
        raw = chimaera_client.get_untangler_overhead_stats()
        stats["untangler"] = _flatten(raw)
    except Exception as exc:
        stats["untangler"] = {"error": str(exc)}

    # Derive combined intercept overhead for convenience
    proxy = stats.get("proxy", {})
    if isinstance(proxy.get("avg_pipeline_overhead_ms"), (int, float)):
        stats["summary"] = {
            "avg_llm_overhead_ms": proxy.get("avg_pipeline_overhead_ms", 0),
            "avg_proxy_record_ms": proxy.get("avg_proxy_overhead_ms", 0),
            "total_requests": proxy.get("total_requests", 0),
            "uptime_seconds": proxy.get("uptime_seconds", 0),
        }

    return jsonify(stats)


@bp.route("/overhead/logging", methods=["GET"])
def get_logging_status():
    """Return current overhead_logging enabled flag for each component."""
    try:
        status = chimaera_client.get_overhead_logging_status()
        return jsonify(status)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503


@bp.route("/overhead/logging", methods=["POST"])
def set_logging():
    """Toggle overhead logging.

    Body: {"enabled": true|false, "component": "all"|"proxy"|"tracker"|"untangler"}
    """
    data = request.get_json(force=True, silent=True) or {}
    enabled = bool(data.get("enabled", True))
    component = data.get("component", "all")
    if component not in ("all", "proxy", "tracker", "untangler"):
        return jsonify({"error": f"unknown component: {component}"}), 400
    try:
        chimaera_client.set_overhead_logging(enabled, component)
        return jsonify({"ok": True, "enabled": enabled, "component": component})
    except Exception as exc:
        return jsonify({"error": str(exc)}), 503


@bp.route("/overhead/dump", methods=["POST"])
def dump_stats():
    """Collect current stats from all components and write to a JSON file.

    Body (optional): {"path": "/abs/path/to/output.json"}
    Defaults to /tmp/cee_overhead_stats.json if path is omitted.
    """
    data = request.get_json(force=True, silent=True) or {}
    out_path = data.get("path", "/tmp/cee_overhead_stats.json")

    # Collect stats
    payload = {
        "generated_at": datetime.datetime.utcnow().isoformat() + "Z",
        "components": {},
    }

    for name, fetch_fn in [
        ("proxy", chimaera_client.get_proxy_dispatch_stats),
        ("tracker", chimaera_client.get_tracker_overhead_stats),
        ("untangler", chimaera_client.get_untangler_overhead_stats),
    ]:
        try:
            raw = fetch_fn()
            payload["components"][name] = _flatten(raw)
        except Exception as exc:
            payload["components"][name] = {"error": str(exc)}

    proxy = payload["components"].get("proxy", {})
    if isinstance(proxy.get("avg_pipeline_overhead_ms"), (int, float)):
        payload["summary"] = {
            "avg_llm_overhead_ms": proxy.get("avg_pipeline_overhead_ms", 0),
            "avg_proxy_record_ms": proxy.get("avg_proxy_overhead_ms", 0),
            "total_requests": proxy.get("total_requests", 0),
            "uptime_seconds": proxy.get("uptime_seconds", 0),
        }

    try:
        os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
        with open(out_path, "w") as fh:
            json.dump(payload, fh, indent=2)
    except OSError as exc:
        return jsonify({"error": f"could not write {out_path}: {exc}"}), 500

    return jsonify({"ok": True, "path": out_path, "stats": payload})
