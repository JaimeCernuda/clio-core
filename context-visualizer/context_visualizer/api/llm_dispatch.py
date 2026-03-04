"""Flask blueprint: HTTP bridge from agent requests to Chimaera Monitor dispatch.

Translates agent HTTP requests into pool_stats://800.0:local:<json> Monitor
queries that land in the proxy ChiMod's Monitor handler on a Chimaera worker.
"""

import json

from flask import Blueprint, Response, request

from .. import chimaera_client

bp = Blueprint("llm_dispatch", __name__)

# Hop-by-hop headers to strip (must not be forwarded)
_HOP_BY_HOP = frozenset({
    "connection", "keep-alive", "te", "trailers",
    "transfer-encoding", "upgrade", "host",
    "content-length", "accept-encoding", "content-encoding",
})

# Session requirement message for agents without /_session/ prefix
_SESSION_MSG = (
    "DTProvenance proxy requires a session ID. "
    "Set ANTHROPIC_BASE_URL to http://<host>:<port>/_session/<your-session-id>"
)


def _detect_provider(path, headers):
    """Detect LLM provider from request path and headers (mirrors C++ DetectProvider)."""
    if path.startswith("/_interceptor"):
        return "unknown"
    if path.startswith("/api/"):
        return "ollama"
    if path == "/v1/messages" or path.startswith("/v1/messages?"):
        return "anthropic"
    if path.startswith("/v1/"):
        if "anthropic-version" in headers:
            return "anthropic"
        return "openai"
    return "unknown"


def _build_session_rejection(provider):
    """Build a provider-appropriate rejection response for missing session."""
    if provider == "anthropic":
        return json.dumps({
            "id": "msg_session_required",
            "type": "message",
            "role": "assistant",
            "model": "dt-provenance-proxy",
            "content": [{"type": "text", "text": _SESSION_MSG}],
            "stop_reason": "end_turn",
            "usage": {"input_tokens": 0, "output_tokens": 0},
        })
    elif provider == "openai":
        return json.dumps({
            "object": "chat.completion",
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": _SESSION_MSG},
                "finish_reason": "stop",
            }],
            "usage": {"prompt_tokens": 0, "completion_tokens": 0, "total_tokens": 0},
        })
    elif provider == "ollama":
        return json.dumps({
            "message": {"role": "assistant", "content": _SESSION_MSG},
            "done": True,
        })
    else:
        return json.dumps({
            "error": {"code": -32000, "message": _SESSION_MSG},
        })


@bp.route("/_session/<session_id>/", defaults={"subpath": ""}, methods=["POST", "GET", "PUT", "DELETE", "OPTIONS"])
@bp.route("/_session/<session_id>/<path:subpath>", methods=["POST", "GET", "PUT", "DELETE", "OPTIONS"])
def forward_request(session_id, subpath):
    """Translate agent HTTP request into Chimaera Monitor query."""
    path = "/" + subpath if subpath else "/"

    # Detect provider from path/headers
    headers_dict = {k.lower(): v for k, v in request.headers}
    provider = _detect_provider(path, headers_dict)

    if provider == "unknown":
        # Default to anthropic for unrecognized paths (agent SDK uses /v1/messages
        # but may send probes to the base URL)
        provider = "anthropic"

    # Filter hop-by-hop headers
    clean_headers = {
        k: v for k, v in request.headers
        if k.lower() not in _HOP_BY_HOP
    }

    # Submit to proxy ChiMod via pool_stats:// Monitor query
    try:
        result = chimaera_client.forward_llm_request(
            session_id=session_id,
            provider=provider,
            path=path,
            headers=clean_headers,
            body=request.get_data(as_text=True),
            timeout=300,
        )
    except Exception as exc:
        return Response(
            json.dumps({"error": f"Chimaera dispatch failed: {exc}"}),
            status=502,
            content_type="application/json",
        )

    # Extract response from container results
    resp = None
    for _cid, data in result.items():
        resp = data
        break

    if resp is None or not isinstance(resp, dict):
        return Response(
            json.dumps({"error": "empty response from proxy ChiMod"}),
            status=502,
            content_type="application/json",
        )

    status = resp.get("status", 502)
    body = resp.get("body", "")

    # Parse response headers from the upstream
    content_type = "application/json"
    resp_headers = {}
    headers_str = resp.get("headers", "{}")
    if isinstance(headers_str, str):
        try:
            resp_headers = json.loads(headers_str)
        except (json.JSONDecodeError, TypeError):
            pass
    elif isinstance(headers_str, dict):
        resp_headers = headers_str

    # Extract content-type, strip hop-by-hop
    for k, v in resp_headers.items():
        if k.lower() == "content-type":
            content_type = v
        elif k.lower() not in _HOP_BY_HOP:
            pass  # Could add to response headers if needed

    return Response(body, status=status, content_type=content_type)


@bp.route("/v1/messages", methods=["POST"])
@bp.route("/v1/chat/completions", methods=["POST"])
def no_session_api():
    """API calls without /_session/ prefix get a fake rejection."""
    headers_dict = {k.lower(): v for k, v in request.headers}
    provider = _detect_provider(request.path, headers_dict)
    return Response(
        _build_session_rejection(provider),
        status=200,
        content_type="application/json",
    )
