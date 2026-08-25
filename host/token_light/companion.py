from __future__ import annotations

from typing import Any


ACTIVITY_TEXT = {
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


def select_pet_pose(
    activity_state: str,
    remaining_percent: int | None,
    *,
    focus_running: bool = False,
    completion_age_seconds: int | None = None,
) -> str:
    if activity_state == "error":
        return "alert"
    if activity_state == "done" and (completion_age_seconds is None or completion_age_seconds <= 30):
        return "celebrate"
    if focus_running:
        return "focus"
    if remaining_percent is not None and remaining_percent <= 10:
        return "tired"
    return {
        "testing": "testing",
        "editing": "coding",
        "thinking": "working",
        "reading": "working",
        "working": "working",
        "waiting": "waiting",
        "idle": "sleep",
        "done": "sleep",
    }.get(activity_state, "sleep")


def _safe_activity(activity: dict[str, Any]) -> dict[str, Any]:
    state = str(activity.get("state") or "idle")
    if state not in ACTIVITY_TEXT:
        state = "idle"
    label, detail = ACTIVITY_TEXT[state]
    elapsed = activity.get("elapsed_seconds", 0)
    sequence = activity.get("completion_seq", 0)
    return {
        "state": state,
        "label": label,
        "detail": detail,
        "elapsed_seconds": max(0, int(elapsed)) if isinstance(elapsed, (int, float)) else 0,
        "completion_seq": max(0, int(sequence)) if isinstance(sequence, (int, float)) else 0,
    }


def build_companion_payload(
    activity: dict[str, Any],
    remaining_percent: int | None,
    *,
    focus_running: bool = False,
    completion_age_seconds: int | None = None,
) -> dict[str, Any]:
    if completion_age_seconds is None:
        internal_age = activity.get("_completion_age_seconds")
        if isinstance(internal_age, (int, float)):
            completion_age_seconds = max(0, int(internal_age))
    safe_activity = _safe_activity(activity)
    pose = select_pet_pose(
        safe_activity["state"],
        remaining_percent,
        focus_running=focus_running,
        completion_age_seconds=completion_age_seconds,
    )
    return {
        "activity": safe_activity,
        "pet": {"pose": pose, "frame_count": 2, "frame_period_ms": 1000},
    }
