from __future__ import annotations

import json
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any


STATE_TEXT = {
    "idle": ("IDLE", "NO ACTIVE TASK"),
    "thinking": ("THINKING", "PLANNING"),
    "reading": ("READING", "RESEARCH"),
    "editing": ("EDITING", "CODE CHANGE"),
    "testing": ("TESTING", "TEST RUN"),
    "working": ("WORKING", "TOOL RUN"),
    "waiting": ("WAITING", "NO RECENT EVENTS"),
    "done": ("DONE", "TASK COMPLETE"),
    "error": ("STOPPED", "TASK STOPPED"),
}

_READ_TOOLS = ("read", "view", "open", "find", "search", "web", "resource", "screenshot")
_EDIT_TOOLS = ("apply_patch", "write", "edit", "replace", "patch")
_TEST_MARKERS = (
    "unittest",
    "pytest",
    "platformio",
    "pio test",
    "pio run",
    "ctest",
    "cargo test",
    "npm test",
    "npm run test",
    "pnpm test",
    "yarn test",
)


def _parse_timestamp(value: Any) -> datetime | None:
    if not isinstance(value, str):
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def _tool_state(name: str, tool_input: Any) -> str:
    lowered_name = name.lower()
    if any(marker in lowered_name for marker in _EDIT_TOOLS):
        return "editing"
    if any(marker in lowered_name for marker in _READ_TOOLS):
        return "reading"

    command = tool_input if isinstance(tool_input, str) else ""
    lowered_command = command.lower()
    if any(marker in lowered_command for marker in _TEST_MARKERS):
        return "testing"
    return "working"


def _event_from_object(obj: Any) -> dict[str, Any] | None:
    if not isinstance(obj, dict):
        return None
    timestamp = _parse_timestamp(obj.get("timestamp"))
    payload = obj.get("payload")
    if timestamp is None or not isinstance(payload, dict):
        return None

    payload_type = payload.get("type")
    if payload_type in {"task_started", "task_complete", "turn_aborted"}:
        return {"timestamp": timestamp, "kind": payload_type}

    if payload_type in {"agent_message", "reasoning"}:
        return {"timestamp": timestamp, "kind": "activity", "state": "thinking"}

    if payload_type in {"custom_tool_call", "function_call"}:
        name = str(payload.get("name") or "")
        tool_input = payload.get("input", payload.get("arguments", ""))
        return {
            "timestamp": timestamp,
            "kind": "activity",
            "state": _tool_state(name, tool_input),
        }
    return None


def _iter_events(codex_home: Path):
    seen: set[tuple[Any, ...]] = set()
    for root_name in ("sessions", "archived_sessions"):
        root = codex_home / root_name
        if not root.is_dir():
            continue
        try:
            paths = root.rglob("*.jsonl")
        except OSError:
            continue
        for path in paths:
            try:
                handle = path.open("r", encoding="utf-8", errors="replace")
            except OSError:
                continue
            with handle:
                for line in handle:
                    try:
                        event = _event_from_object(json.loads(line))
                    except (json.JSONDecodeError, OSError):
                        continue
                    if event is None:
                        continue
                    key = (event["timestamp"], event["kind"], event.get("state"))
                    if key in seen:
                        continue
                    seen.add(key)
                    yield event


def _result(state: str, elapsed_seconds: int, completion_seq: int) -> dict[str, Any]:
    label, detail = STATE_TEXT[state]
    return {
        "state": state,
        "label": label,
        "detail": detail,
        "elapsed_seconds": max(0, elapsed_seconds),
        "completion_seq": max(0, completion_seq),
    }


def read_codex_activity(codex_home: Path | str, now: datetime) -> dict[str, Any]:
    """Return a fixed, privacy-safe activity summary from local Codex logs."""
    home = Path(codex_home).expanduser()
    current = now if now.tzinfo is not None else now.replace(tzinfo=timezone.utc)
    current = current.astimezone(timezone.utc)
    events = sorted(_iter_events(home), key=lambda event: event["timestamp"])
    events = [event for event in events if event["timestamp"] <= current]
    completion_seq = sum(event["kind"] == "task_complete" for event in events)
    if not events:
        return _result("idle", 0, completion_seq)

    task_start_index = -1
    terminal_index = -1
    terminal_start_index = -1
    for index, event in enumerate(events):
        if event["kind"] == "task_started":
            task_start_index = index
        elif event["kind"] in {"task_complete", "turn_aborted"}:
            terminal_index = index
            terminal_start_index = task_start_index
            task_start_index = -1

    if task_start_index >= 0:
        active_events = events[task_start_index:]
        start_at = active_events[0]["timestamp"]
        latest = active_events[-1]
        if latest["kind"] == "task_complete":
            elapsed = int((latest["timestamp"] - start_at).total_seconds())
            if current - latest["timestamp"] <= timedelta(minutes=2):
                return _result("done", elapsed, completion_seq)
            return _result("idle", 0, completion_seq)
        if latest["kind"] == "turn_aborted":
            elapsed = int((latest["timestamp"] - start_at).total_seconds())
            if current - latest["timestamp"] <= timedelta(minutes=2):
                return _result("error", elapsed, completion_seq)
            return _result("idle", 0, completion_seq)

        elapsed = int((current - start_at).total_seconds())
        if current - latest["timestamp"] >= timedelta(minutes=5):
            return _result("waiting", elapsed, completion_seq)
        state = "thinking"
        for event in active_events:
            if event.get("state"):
                state = event["state"]
        return _result(state, elapsed, completion_seq)

    if terminal_index >= 0:
        terminal = events[terminal_index]
        state = "done" if terminal["kind"] == "task_complete" else "error"
        if current - terminal["timestamp"] <= timedelta(minutes=2):
            elapsed = 0
            if terminal_start_index >= 0:
                elapsed = int(
                    (terminal["timestamp"] - events[terminal_start_index]["timestamp"]).total_seconds()
                )
            result = _result(state, elapsed, completion_seq)
            if state == "done":
                result["_completion_age_seconds"] = max(
                    0, int((current - terminal["timestamp"]).total_seconds())
                )
            return result
    return _result("idle", 0, completion_seq)
