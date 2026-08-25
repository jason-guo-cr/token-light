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


def _bucket_label(window: UsageWindow) -> str:
    if window.limit_id == "codex" or not window.limit_name:
        return "CODEX"

    name = window.limit_name.upper()
    for prefix in ("GPT-5.3-CODEX-", "GPT-5.6-CODEX-", "GPT-CODEX-", "CODEX-"):
        if name.startswith(prefix):
            name = name.removeprefix(prefix)
            break
    compact = "".join(char for char in name if char.isalnum() or char == " ").strip()
    return (compact or window.limit_id.upper())[:8]


def _period_label(window_minutes: int) -> str:
    if window_minutes == 10080:
        return "WEEK"
    if window_minutes % 1440 == 0:
        return f"{window_minutes // 1440}D"
    hours = max(1, round(window_minutes / 60))
    return f"{hours}H"


def _label_for_window(window: UsageWindow) -> str:
    return f"{_bucket_label(window)} {_period_label(window.window_minutes)}"


def _reset_label(reset_at: int, now: datetime) -> str:
    reset_dt = datetime.fromtimestamp(reset_at, tz=LOCAL_TZ)
    local_now = _as_local(now)
    if reset_dt.date() == local_now.date():
        return reset_dt.strftime("%H:%M")
    return reset_dt.strftime("%m/%d")


def _window_snapshot(window: UsageWindow, now: datetime) -> dict:
    return {
        "label": _label_for_window(window),
        "remaining_percent": window.remaining_percent,
        "used_percent": window.used_percent,
        "reset_label": _reset_label(window.reset_at, now),
        "reset_at": window.reset_at,
        "window_minutes": window.window_minutes,
        "limit_id": window.limit_id,
    }


def display_window(usage: UsageStatus) -> UsageWindow:
    general = [window for window in usage.windows if window.limit_id == "codex"]
    candidates = general or list(usage.windows)
    return max(candidates, key=lambda window: window.window_minutes)


def build_snapshot(
    usage: UsageStatus,
    now: datetime | None = None,
    battery: dict | None = None,
    token_usage: dict | None = None,
    limit_updated_at: datetime | None = None,
    weather: dict | None = None,
    status: str = "live",
    warning: str | None = None,
    companion: dict | None = None,
    forecast: dict | None = None,
    audio: dict | None = None,
) -> dict:
    current = now or _now()
    primary = _window_snapshot(display_window(usage), current)
    snapshot = _base_snapshot(current)
    snapshot.update(
        {
            "plan_type": usage.plan_type,
            "primary": primary,
            "status": status,
        }
    )
    if warning:
        snapshot["warning"] = warning
    if battery is not None:
        snapshot["battery"] = battery
    if token_usage is not None:
        snapshot["token_usage"] = token_usage
    if limit_updated_at is not None:
        snapshot["limit_updated_label"] = _as_local(limit_updated_at).strftime("%H:%M")
    if weather is not None:
        snapshot["weather"] = weather
    if companion is not None:
        snapshot["companion"] = companion
    if forecast is not None:
        snapshot["forecast"] = forecast
    if audio is not None:
        snapshot["audio"] = audio
    return snapshot


def build_error_snapshot(
    message: str,
    now: datetime | None = None,
    battery: dict | None = None,
    token_usage: dict | None = None,
    limit_updated_at: datetime | None = None,
    weather: dict | None = None,
    companion: dict | None = None,
    forecast: dict | None = None,
    audio: dict | None = None,
) -> dict:
    snapshot = _base_snapshot(now or _now())
    snapshot.update({"status": "api_error", "message": message})
    if battery is not None:
        snapshot["battery"] = battery
    if token_usage is not None:
        snapshot["token_usage"] = token_usage
    if limit_updated_at is not None:
        snapshot["limit_updated_label"] = _as_local(limit_updated_at).strftime("%H:%M")
    if weather is not None:
        snapshot["weather"] = weather
    if companion is not None:
        snapshot["companion"] = companion
    if forecast is not None:
        snapshot["forecast"] = forecast
    if audio is not None:
        snapshot["audio"] = audio
    return snapshot
