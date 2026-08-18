from fastapi.testclient import TestClient

import pytest

from app.main import AttentionCancelRequest, AttentionRequest, app, store


client = TestClient(app)


@pytest.fixture(autouse=True)
def reset_store() -> None:
    store.reset()


def test_status_can_be_read_and_changed() -> None:
    update = {"state": "working", "message": "Running tests"}
    response = client.post("/api/v1/debug/status", json=update)

    assert response.status_code == 200
    assert response.json() == {"agent": "demo", **update}
    assert client.get("/api/v1/status").json() == {"agent": "demo", **update}


def test_unknown_state_is_rejected() -> None:
    response = client.post(
        "/api/v1/debug/status",
        json={"state": "paused", "message": "unsupported"},
    )

    assert response.status_code == 422


def test_adapter_can_publish_agent_status() -> None:
    update = {
        "agent": "openclaw",
        "state": "completed",
        "message": "OpenClaw run completed",
    }

    response = client.post("/api/v1/status", json=update)

    assert response.status_code == 200
    assert response.json() == update
    assert client.get("/api/v1/status").json() == update


def test_agents_lists_each_latest_status_without_changing_legacy_status() -> None:
    agent_a = {"agent": "agent-a", "state": "working", "message": "Core2"}
    agent_b = {"agent": "agent-b", "state": "idle", "message": "Ready"}

    client.post("/api/v1/status", json=agent_a)
    client.post("/api/v1/status", json=agent_b)

    assert client.get("/api/v1/status").json() == agent_b
    assert client.get("/api/v1/agents").json() == [
        agent_a,
        agent_b,
    ]


def test_agents_applies_attention_only_to_its_agent_and_restores_status() -> None:
    client.post(
        "/api/v1/status",
        json={"agent": "agent-a", "state": "working", "message": "Building"},
    )
    created = client.post(
        "/api/v1/attention",
        json={"agent": "agent-b", "reason": "user_input", "message": "Choose"},
    ).json()

    agents = {item["agent"]: item for item in client.get("/api/v1/agents").json()}
    assert agents["agent-a"]["state"] == "working"
    assert agents["agent-b"] == {
        "agent": "agent-b",
        "state": "human_required",
        "message": "Choose",
    }

    client.post(
        "/api/v1/attention/clear", json={"id": created["attention"]["id"]}
    )
    agents = {item["agent"]: item for item in client.get("/api/v1/agents").json()}
    assert "agent-b" not in agents
    assert agents["agent-a"]["state"] == "working"


def test_agents_orders_multiple_human_requests_fifo() -> None:
    client.post(
        "/api/v1/attention",
        json={"agent": "agent-b", "reason": "user_input", "message": "First"},
    )
    client.post(
        "/api/v1/attention",
        json={"agent": "agent-a", "reason": "judgement", "message": "Second"},
    )

    agents = client.get("/api/v1/agents").json()
    assert [item["agent"] for item in agents[:2]] == ["agent-b", "agent-a"]
    assert all(item["state"] == "human_required" for item in agents[:2])


def test_adapter_publish_requires_agent() -> None:
    response = client.post(
        "/api/v1/status",
        json={"state": "working", "message": "Missing adapter name"},
    )

    assert response.status_code == 422


def test_adapter_publish_rejects_unknown_state() -> None:
    response = client.post(
        "/api/v1/status",
        json={"agent": "openclaw", "state": "paused", "message": "Paused"},
    )

    assert response.status_code == 422


def test_health() -> None:
    response = client.get("/healthz")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_attention_overrides_and_then_restores_latest_agent_state() -> None:
    client.post(
        "/api/v1/status",
        json={"agent": "openclaw", "state": "working", "message": "Running"},
    )
    created = client.post(
        "/api/v1/attention",
        json={
            "agent": "openclaw",
            "reason": "physical_check",
            "message": "Check the ESP32 LED",
        },
    )

    assert created.status_code == 200
    body = created.json()
    attention_id = body["attention"]["id"]
    assert body["pending_count"] == 1
    assert body["status"] == {
        "agent": "openclaw",
        "state": "human_required",
        "message": "Check the ESP32 LED",
    }

    # Agent state continues to advance underneath the attention overlay.
    client.post(
        "/api/v1/status",
        json={"agent": "openclaw", "state": "completed", "message": "Done"},
    )
    cleared = client.post("/api/v1/attention/clear", json={"id": attention_id})

    assert cleared.status_code == 200
    assert cleared.json()["cleared"] is True
    assert cleared.json()["status"] == {
        "agent": "openclaw",
        "state": "completed",
        "message": "Done",
    }


def test_multiple_attention_requests_are_cleared_by_id() -> None:
    first = client.post(
        "/api/v1/attention",
        json={"agent": "a", "reason": "user_input", "message": "First"},
    ).json()
    second = client.post(
        "/api/v1/attention",
        json={"agent": "b", "reason": "judgement", "message": "Second"},
    ).json()

    assert second["pending_count"] == 2
    assert client.get("/api/v1/status").json()["message"] == "First"
    assert len(client.get("/api/v1/attention").json()) == 2

    first_clear = client.post(
        "/api/v1/attention/clear", json={"id": first["attention"]["id"]}
    ).json()
    assert first_clear["pending_count"] == 1
    assert first_clear["status"]["state"] == "human_required"
    assert first_clear["status"]["message"] == "Second"

    second_clear = client.post(
        "/api/v1/attention/clear", json={"id": second["attention"]["id"]}
    ).json()
    assert second_clear["pending_count"] == 0
    assert second_clear["status"]["state"] == "idle"


def test_attention_clear_reveals_pending_approval_state() -> None:
    client.post(
        "/api/v1/status",
        json={
            "agent": "openclaw",
            "state": "waiting_approval",
            "message": "Approval pending",
        },
    )
    created = client.post(
        "/api/v1/attention",
        json={"agent": "other-agent", "reason": "manual_action", "message": "Act"},
    ).json()

    cleared = client.post(
        "/api/v1/attention/clear", json={"id": created["attention"]["id"]}
    ).json()

    assert cleared["status"] == {
        "agent": "openclaw",
        "state": "waiting_approval",
        "message": "Approval pending",
    }


def test_attention_reason_is_validated_and_unknown_clear_is_idempotent() -> None:
    invalid = client.post(
        "/api/v1/attention",
        json={"agent": "a", "reason": "coffee", "message": "Invalid"},
    )
    assert invalid.status_code == 422

    cleared = client.post("/api/v1/attention/clear", json={"id": "missing"})
    assert cleared.status_code == 200
    assert cleared.json()["cleared"] is False
    assert cleared.json()["pending_count"] == 0


def test_expired_attention_is_not_returned_or_shown() -> None:
    now = [1_000]
    expiring_store = type(store)(clock=lambda: now[0], attention_ttl_seconds=60)
    created = expiring_store.add_attention(
        AttentionRequest(
            agent="codex", reason="physical_check", message="Check display"
        )
    )
    assert created.attention.created_at == 1_000
    assert created.attention.expires_at == 1_060

    now[0] = 1_060
    assert expiring_store.list_attention() == []
    assert expiring_store.get().state == "idle"
    history = expiring_store.list_attention_history()
    assert len(history) == 1
    assert history[0].status == "expired"
    assert history[0].terminal_reason == "ttl"


def test_attention_records_owner_and_explicit_clear_history() -> None:
    created = client.post(
        "/api/v1/attention",
        json={
            "agent": "codex",
            "reason": "physical_check",
            "message": "Check display",
            "origin_session_id": "codex-mcp:session-a",
            "run_id": "run-a",
        },
    ).json()
    attention = created["attention"]
    assert attention["status"] == "pending"
    assert attention["origin_session_id"] == "codex-mcp:session-a"
    assert attention["run_id"] == "run-a"
    assert attention["expires_at"] == attention["created_at"] + 14400

    cleared = client.post(
        "/api/v1/attention/clear",
        json={
            "id": attention["id"],
            "origin_session_id": "codex-mcp:session-a",
            "run_id": "run-a",
        },
    ).json()
    assert cleared["cleared"] is True
    assert cleared["attention_status"] == "resolved"
    history = client.get("/api/v1/attention/history").json()
    assert history[-1]["status"] == "resolved"
    assert history[-1]["terminal_reason"] == "explicit_clear"
    assert client.get("/api/v1/attention").json() == []


def test_wrong_owner_cannot_clear_another_session_attention() -> None:
    created = client.post(
        "/api/v1/attention",
        json={
            "agent": "codex",
            "reason": "physical_check",
            "message": "Session A",
            "origin_session_id": "codex-mcp:session-a",
        },
    ).json()
    rejected = client.post(
        "/api/v1/attention/clear",
        json={
            "id": created["attention"]["id"],
            "origin_session_id": "codex-mcp:session-b",
        },
    ).json()
    assert rejected["cleared"] is False
    assert len(client.get("/api/v1/attention").json()) == 1
    assert client.get("/api/v1/attention/history").json() == []


def test_owner_cancel_is_scoped_and_preserves_other_sessions_and_agents() -> None:
    first = client.post(
        "/api/v1/attention",
        json={
            "agent": "codex",
            "reason": "physical_check",
            "message": "Session A",
            "origin_session_id": "codex-mcp:session-a",
        },
    ).json()["attention"]["id"]
    second = client.post(
        "/api/v1/attention",
        json={
            "agent": "codex",
            "reason": "physical_check",
            "message": "Session B",
            "origin_session_id": "codex-mcp:session-b",
        },
    ).json()["attention"]["id"]
    agent_a = client.post(
        "/api/v1/attention",
        json={
            "agent": "agent-a",
            "reason": "judgement",
            "message": "Agent A",
            "origin_session_id": "openclaw:session-a",
        },
    ).json()["attention"]["id"]

    cancelled = client.post(
        "/api/v1/attention/cancel",
        json={"origin_session_id": "codex-mcp:session-a"},
    ).json()
    assert cancelled["cancelled_ids"] == [first]
    remaining = client.get("/api/v1/attention").json()
    assert [item["id"] for item in remaining] == [second, agent_a]
    history = client.get("/api/v1/attention/history").json()
    assert history[-1]["id"] == first
    assert history[-1]["status"] == "cancelled"


def test_cancel_requires_an_owner_identity() -> None:
    response = client.post("/api/v1/attention/cancel", json={})
    assert response.status_code == 422


def test_legacy_id_only_clear_remains_compatible() -> None:
    created = client.post(
        "/api/v1/attention",
        json={"agent": "agent-a", "reason": "user_input", "message": "Choose"},
    ).json()
    cleared = client.post(
        "/api/v1/attention/clear", json={"id": created["attention"]["id"]}
    ).json()
    assert cleared["cleared"] is True
