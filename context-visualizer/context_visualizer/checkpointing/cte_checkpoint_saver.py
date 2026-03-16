"""CTE-backed LangGraph checkpoint saver.

Stores LangGraph checkpoints in Chimaera CTE blobs, enabling durable
cross-restart state for LangGraph agents whose calls are intercepted
by DTProvenance.
"""
from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any, Dict, Iterator, Optional

try:
    from langgraph.checkpoint.base import BaseCheckpointSaver, CheckpointTuple
    from langchain_core.runnables import RunnableConfig
    _LANGGRAPH_AVAILABLE = True
except ImportError:
    _LANGGRAPH_AVAILABLE = False
    BaseCheckpointSaver = object
    CheckpointTuple = None
    RunnableConfig = dict

from .. import chimaera_client


class CTECheckpointSaver(BaseCheckpointSaver):
    """LangGraph checkpoint saver backed by Chimaera CTE blob storage.

    Each thread's checkpoints are stored under the CTE tag
    ``LG_checkpoint_<thread_id>`` with monotonically increasing blob names
    (zero-padded 10-digit integers) so that ``list()`` / "get latest" work
    correctly.
    """

    def put(
        self,
        config: RunnableConfig,
        checkpoint: Dict[str, Any],
        metadata: Dict[str, Any],
        new_versions: Dict[str, Any],
    ) -> RunnableConfig:
        thread_id = config["configurable"]["thread_id"]
        tag = f"LG_checkpoint_{thread_id}"
        data = {
            "checkpoint_id": checkpoint["id"],
            "checkpoint": checkpoint,
            "metadata": metadata,
            "ts": datetime.utcnow().isoformat() + "Z",
        }
        blob_name = chimaera_client.store_lg_checkpoint(tag, data)
        return {
            **config,
            "configurable": {
                **config["configurable"],
                "checkpoint_id": checkpoint["id"],
            },
        }

    def get_tuple(self, config: RunnableConfig) -> Optional[CheckpointTuple]:
        thread_id = config["configurable"]["thread_id"]
        tag = f"LG_checkpoint_{thread_id}"
        try:
            pairs = chimaera_client.query_lg_checkpoints(tag)
        except Exception:
            return None
        if not pairs:
            return None

        target_id = config["configurable"].get("checkpoint_id")
        if target_id:
            for blob_name, data in pairs:
                if data.get("checkpoint_id") == target_id:
                    return self._to_tuple(config, data)
            return None
        # Return latest (last element — pairs are sorted by blob name ascending)
        _, data = pairs[-1]
        return self._to_tuple(config, data)

    def list(
        self,
        config: RunnableConfig,
        *,
        filter: Optional[Dict[str, Any]] = None,
        before: Optional[RunnableConfig] = None,
        limit: Optional[int] = None,
    ) -> Iterator[CheckpointTuple]:
        thread_id = config["configurable"]["thread_id"]
        tag = f"LG_checkpoint_{thread_id}"
        try:
            pairs = chimaera_client.query_lg_checkpoints(tag)
        except Exception:
            return

        # Newest-first
        pairs = list(reversed(pairs))

        before_id = None
        if before is not None:
            before_id = before["configurable"].get("checkpoint_id")

        count = 0
        for blob_name, data in pairs:
            if before_id and data.get("checkpoint_id") == before_id:
                continue
            if limit is not None and count >= limit:
                break
            tup = self._to_tuple(config, data)
            if tup is not None:
                yield tup
                count += 1

    def _to_tuple(self, config: RunnableConfig, data: Dict[str, Any]) -> Optional[CheckpointTuple]:
        if CheckpointTuple is None:
            return None
        checkpoint = data.get("checkpoint", {})
        metadata = data.get("metadata", {})
        checkpoint_id = data.get("checkpoint_id", checkpoint.get("id", ""))
        parent_id = metadata.get("parent_id")
        parent_config = None
        if parent_id:
            parent_config = {
                **config,
                "configurable": {**config["configurable"], "checkpoint_id": parent_id},
            }
        return CheckpointTuple(
            config={
                **config,
                "configurable": {
                    **config["configurable"],
                    "checkpoint_id": checkpoint_id,
                },
            },
            checkpoint=checkpoint,
            metadata=metadata,
            parent_config=parent_config,
        )

    @classmethod
    def from_interaction(cls, interaction_record: dict) -> dict:
        """Reconstruct a LangGraph-compatible checkpoint from an intercepted InteractionRecord.

        Args:
            interaction_record: A dict as returned by chimaera_client.get_interaction()
                                 (JSON-decoded InteractionRecord).

        Returns:
            A checkpoint dict compatible with LangGraph's checkpoint format,
            with ``channel_values.messages`` populated from the interaction's
            request messages.
        """
        messages = (
            interaction_record
            .get("request", {})
            .get("body", {})
            .get("messages", [])
        )
        return {
            "id": str(uuid.uuid4()),
            "v": 1,
            "ts": interaction_record.get("timestamp", datetime.utcnow().isoformat() + "Z"),
            "channel_values": {"messages": messages},
            "channel_versions": {},
            "versions_seen": {},
            "pending_sends": [],
        }
