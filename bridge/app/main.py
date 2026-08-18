import os
import time
from threading import Lock
from typing import Literal
from uuid import uuid4

from fastapi import FastAPI
from pydantic import BaseModel, Field, model_validator


AgentState = Literal[
    "idle",
    "working",
    "waiting_approval",
    "human_required",
    "completed",
    "error",
]
AttentionReason = Literal[
    "approval",
    "physical_check",
    "user_input",
    "manual_action",
    "judgement",
    "other",
]
AttentionLifecycle = Literal["pending", "resolved", "cancelled", "expired"]


class Status(BaseModel):
    agent: str
    state: AgentState
    message: str


class StatusUpdate(BaseModel):
    state: AgentState
    message: str = Field(default="", max_length=500)


class AdapterStatusUpdate(StatusUpdate):
    agent: str = Field(min_length=1, max_length=100)


class AttentionRequest(BaseModel):
    agent: str = Field(min_length=1, max_length=100)
    reason: AttentionReason
    message: str = Field(min_length=1, max_length=500)
    origin_session_id: str | None = Field(default=None, min_length=1, max_length=200)
    run_id: str | None = Field(default=None, min_length=1, max_length=200)


class AttentionClearRequest(BaseModel):
    id: str = Field(min_length=1, max_length=100)
    origin_session_id: str | None = Field(default=None, min_length=1, max_length=200)
    run_id: str | None = Field(default=None, min_length=1, max_length=200)


class AttentionCancelRequest(BaseModel):
    id: str | None = Field(default=None, min_length=1, max_length=100)
    agent: str | None = Field(default=None, min_length=1, max_length=100)
    origin_session_id: str | None = Field(default=None, min_length=1, max_length=200)
    run_id: str | None = Field(default=None, min_length=1, max_length=200)

    @model_validator(mode="after")
    def has_owner(self) -> "AttentionCancelRequest":
        if self.origin_session_id is None and self.run_id is None:
            raise ValueError("cancel requires origin_session_id or run_id")
        return self


class Attention(BaseModel):
    id: str
    agent: str
    reason: AttentionReason
    message: str
    origin_session_id: str | None = None
    run_id: str | None = None
    created_at: int
    expires_at: int
    status: AttentionLifecycle = "pending"
    resolved_at: int | None = None
    terminal_reason: str | None = None


class AttentionCreateResponse(BaseModel):
    attention: Attention
    pending_count: int
    status: Status


class AttentionClearResponse(BaseModel):
    id: str
    cleared: bool
    pending_count: int
    status: Status
    attention_status: AttentionLifecycle | None = None


class AttentionCancelResponse(BaseModel):
    cancelled_ids: list[str]
    cancelled_count: int
    pending_count: int
    status: Status


class StatusStore:
    def __init__(self, *, clock=time.time, attention_ttl_seconds: int | None = None) -> None:
        self._lock = Lock()
        self._clock = clock
        self._attention_ttl_seconds = (
            attention_ttl_seconds
            if attention_ttl_seconds is not None
            else _attention_ttl_seconds_from_environment()
        )
        self._base_status = Status(agent="demo", state="idle", message="Ready")
        self._agent_status: dict[str, Status] = {
            self._base_status.agent: self._base_status.model_copy()
        }
        self._attention: dict[str, Attention] = {}
        self._attention_history: list[Attention] = []

    def _discard_expired_attention(self) -> None:
        now = int(self._clock())
        expired_ids = [
            attention_id
            for attention_id, attention in self._attention.items()
            if attention.expires_at <= now
        ]
        for attention_id in expired_ids:
            attention = self._attention.pop(attention_id)
            self._attention_history.append(
                attention.model_copy(
                    update={
                        "status": "expired",
                        "resolved_at": now,
                        "terminal_reason": "ttl",
                    }
                )
            )

    @staticmethod
    def _owner_matches(
        attention: Attention,
        *,
        agent: str | None = None,
        origin_session_id: str | None = None,
        run_id: str | None = None,
    ) -> bool:
        return (
            (agent is None or attention.agent == agent)
            and (
                origin_session_id is None
                or attention.origin_session_id == origin_session_id
            )
            and (run_id is None or attention.run_id == run_id)
        )

    def _finish_attention(
        self,
        attention_id: str,
        *,
        lifecycle: AttentionLifecycle,
        now: int,
        terminal_reason: str,
    ) -> Attention | None:
        attention = self._attention.pop(attention_id, None)
        if attention is None:
            return None
        finished = attention.model_copy(
            update={
                "status": lifecycle,
                "resolved_at": now,
                "terminal_reason": terminal_reason,
            }
        )
        self._attention_history.append(finished)
        return finished

    def _effective_status(self) -> Status:
        self._discard_expired_attention()
        if self._attention:
            attention = next(iter(self._attention.values()))
            return Status(
                agent=attention.agent,
                state="human_required",
                message=attention.message,
            )
        return self._base_status.model_copy()

    def get(self) -> Status:
        with self._lock:
            return self._effective_status()

    def update(self, update: StatusUpdate, agent: str | None = None) -> Status:
        with self._lock:
            self._base_status = Status(
                agent=agent or self._base_status.agent,
                state=update.state,
                message=update.message,
            )
            self._agent_status[self._base_status.agent] = self._base_status.model_copy()
            return self._effective_status()

    def list_agents(self) -> list[Status]:
        """Return one effective status per known agent; attention is FIFO."""
        with self._lock:
            self._discard_expired_attention()
            statuses: dict[str, Status] = {}
            # Attention is an overlay per agent. The first pending request for
            # that agent wins. Attention agents are inserted first in global
            # FIFO order so clients can use response order as a tie-breaker.
            for attention in self._attention.values():
                if attention.agent in statuses:
                    continue
                statuses[attention.agent] = Status(
                    agent=attention.agent,
                    state="human_required",
                    message=attention.message,
                )
            for agent, status in self._agent_status.items():
                if agent not in statuses:
                    statuses[agent] = status.model_copy()
            if len(statuses) > 1:
                demo = statuses.get("demo")
                if demo == Status(agent="demo", state="idle", message="Ready"):
                    statuses.pop("demo")
            return list(statuses.values())

    def add_attention(self, request: AttentionRequest) -> AttentionCreateResponse:
        with self._lock:
            self._discard_expired_attention()
            created_at = int(self._clock())
            attention = Attention(
                id=str(uuid4()),
                created_at=created_at,
                expires_at=created_at + self._attention_ttl_seconds,
                **request.model_dump(),
            )
            self._attention[attention.id] = attention
            return AttentionCreateResponse(
                attention=attention.model_copy(),
                pending_count=len(self._attention),
                status=self._effective_status(),
            )

    def clear_attention(
        self, request: AttentionClearRequest | str
    ) -> AttentionClearResponse:
        with self._lock:
            if isinstance(request, str):
                request = AttentionClearRequest(id=request)
            self._discard_expired_attention()
            now = int(self._clock())
            attention = self._attention.get(request.id)
            owner_matched = attention is not None and self._owner_matches(
                attention,
                origin_session_id=request.origin_session_id,
                run_id=request.run_id,
            )
            cleared_attention = (
                self._finish_attention(
                    request.id,
                    lifecycle="resolved",
                    now=now,
                    terminal_reason="explicit_clear",
                )
                if owner_matched
                else None
            )
            return AttentionClearResponse(
                id=request.id,
                cleared=cleared_attention is not None,
                pending_count=len(self._attention),
                status=self._effective_status(),
                attention_status=(
                    cleared_attention.status if cleared_attention is not None else None
                ),
            )

    def cancel_attention(self, request: AttentionCancelRequest) -> AttentionCancelResponse:
        with self._lock:
            self._discard_expired_attention()
            now = int(self._clock())
            candidates = list(self._attention.values())
            cancelled_ids: list[str] = []
            for attention in candidates:
                if request.id is not None and attention.id != request.id:
                    continue
                if not self._owner_matches(
                    attention,
                    agent=request.agent,
                    origin_session_id=request.origin_session_id,
                    run_id=request.run_id,
                ):
                    continue
                if self._finish_attention(
                    attention.id,
                    lifecycle="cancelled",
                    now=now,
                    terminal_reason="owner_lifecycle_end",
                ) is not None:
                    cancelled_ids.append(attention.id)
            return AttentionCancelResponse(
                cancelled_ids=cancelled_ids,
                cancelled_count=len(cancelled_ids),
                pending_count=len(self._attention),
                status=self._effective_status(),
            )

    def list_attention(self) -> list[Attention]:
        with self._lock:
            self._discard_expired_attention()
            return [attention.model_copy() for attention in self._attention.values()]

    def list_attention_history(self) -> list[Attention]:
        with self._lock:
            self._discard_expired_attention()
            return [attention.model_copy() for attention in self._attention_history]

    def reset(self) -> None:
        with self._lock:
            self._base_status = Status(agent="demo", state="idle", message="Ready")
            self._agent_status = {
                self._base_status.agent: self._base_status.model_copy()
            }
            self._attention.clear()
            self._attention_history.clear()


def _attention_ttl_seconds_from_environment() -> int:
    value = os.getenv("STATELAMP_ATTENTION_TTL_SECONDS", "14400")
    try:
        seconds = int(value)
    except ValueError:
        return 14400
    return max(60, min(seconds, 86400))


store = StatusStore()
app = FastAPI(title="StateLamp Bridge", version="0.2.0")


@app.get("/api/v1/status", response_model=Status)
def get_status() -> Status:
    return store.get()


@app.get("/api/v1/agents", response_model=list[Status])
def list_agents() -> list[Status]:
    return store.list_agents()


@app.post("/api/v1/debug/status", response_model=Status)
def set_debug_status(update: StatusUpdate) -> Status:
    return store.update(update)


@app.post("/api/v1/status", response_model=Status)
def set_adapter_status(update: AdapterStatusUpdate) -> Status:
    return store.update(update, agent=update.agent)


@app.get("/api/v1/attention", response_model=list[Attention])
def list_attention() -> list[Attention]:
    return store.list_attention()


@app.post("/api/v1/attention", response_model=AttentionCreateResponse)
def request_attention(request: AttentionRequest) -> AttentionCreateResponse:
    return store.add_attention(request)


@app.post("/api/v1/attention/clear", response_model=AttentionClearResponse)
def clear_attention(request: AttentionClearRequest) -> AttentionClearResponse:
    return store.clear_attention(request)


@app.post("/api/v1/attention/cancel", response_model=AttentionCancelResponse)
def cancel_attention(request: AttentionCancelRequest) -> AttentionCancelResponse:
    return store.cancel_attention(request)


@app.get("/api/v1/attention/history", response_model=list[Attention])
def attention_history() -> list[Attention]:
    return store.list_attention_history()


@app.get("/healthz")
def health() -> dict[str, str]:
    return {"status": "ok"}
