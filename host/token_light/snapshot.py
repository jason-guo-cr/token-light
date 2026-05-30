from __future__ import annotations

from datetime import datetime
from zoneinfo import ZoneInfo

from token_light.codex_usage import UsageStatus, UsageWindow


LOCAL_TZ = ZoneInfo("Asia/Shanghai")
WEEKDAYS = ("MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN")


def _now() -> datetime:
    return datetime.now(tz=LOCAL_TZ)


def _as_local(now: datetime) -> datetime:
    if now.tzinfo is None:
        return now.replace(tzinfo=LOCAL_TZ)
    return now.astimezone(LOCAL_TZ)


def _base_snapshot(now: datetime) -> dict:
    local = _as_local(now)
    return {
        "type": "snapshot",
        "sent_at": local.isoformat(timespec="seconds"),
        "date": local.strftime("%m/%d"),
        "weekday": WEEKDAYS[local.weekday()],
        "time": local.strftime("%H:%M"),
    }


def _label_for_window(window_minutes: int) -> str:
    if window_minutes == 300:
        return "5H LIMIT"
    if window_minutes == 10080:
        return "WEEK LIMIT"
    hours = max(1, round(window_minutes / 60))
    return f"{hours}H LIMIT"


def _reset_label(reset_at: int, now: datetime) -> str:
    reset_dt = datetime.fromtimestamp(reset_at, tz=LOCAL_TZ)
    local_now = _as_local(now)
    if reset_dt.date() == local_now.date():
        return reset_dt.strftime("%H:%M")
    return reset_dt.strftime("%m/%d")


def _window_snapshot(window: UsageWindow, now: datetime) -> dict:
    return {
        "label": _label_for_window(window.window_minutes),
        "remaining_percent": window.remaining_percent,
        "used_percent": window.used_percent,
        "reset_label": _reset_label(window.reset_at, now),
        "reset_at": window.reset_at,
        "window_minutes": window.window_minutes,
    }


def build_snapshot(usage: UsageStatus, now: datetime | None = None, battery: dict | None = None) -> dict:
    current = now or _now()
    snapshot = _base_snapshot(current)
    snapshot.update(
        {
            "plan_type": usage.plan_type,
            "primary": _window_snapshot(usage.primary, current),
            "secondary": _window_snapshot(usage.secondary, current),
            "status": "live",
        }
    )
    if battery is not None:
        snapshot["battery"] = battery
    return snapshot


def build_error_snapshot(message: str, now: datetime | None = None, battery: dict | None = None) -> dict:
    snapshot = _base_snapshot(now or _now())
    snapshot.update({"status": "api_error", "message": message})
    if battery is not None:
        snapshot["battery"] = battery
    return snapshot
