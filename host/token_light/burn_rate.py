from __future__ import annotations

import json
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterable


def _timestamp(value: Any) -> datetime | None:
    if isinstance(value, datetime):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        except ValueError:
            return None
    else:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def _compact_per_hour(value: int) -> str:
    if value == 0:
        return "0/H"
    if value >= 1_000_000_000:
        return f"{value / 1_000_000_000:.1f}B/H"
    if value >= 1_000_000:
        return f"{value / 1_000_000:.1f}M/H"
    if value >= 10_000:
        return f"{value / 1_000:.0f}K/H"
    if value >= 1_000:
        return f"{value / 1_000:.1f}K/H"
    return f"{value}/H"


def load_quota_history(path: Path | str) -> list[dict[str, Any]]:
    history_path = Path(path).expanduser()
    try:
        handle = history_path.open("r", encoding="utf-8", errors="replace")
    except OSError:
        return []
    samples: list[dict[str, Any]] = []
    with handle:
        for line in handle:
            try:
                sample = json.loads(line)
            except json.JSONDecodeError:
                continue
            if not isinstance(sample, dict):
                continue
            if _timestamp(sample.get("observed_at")) is None:
                continue
            if not isinstance(sample.get("limit_id"), str):
                continue
            if not isinstance(sample.get("reset_at"), int):
                continue
            if not isinstance(sample.get("used_percent"), (int, float)):
                continue
            samples.append(sample)
    return samples


def _quota_projection(
    samples: Iterable[dict[str, Any]], current_window: dict[str, Any] | None, now: datetime
) -> tuple[float | None, int | None]:
    if not current_window:
        return None, None
    limit_id = current_window.get("limit_id")
    reset_at = current_window.get("reset_at")
    used_percent = current_window.get("used_percent")
    if not isinstance(limit_id, str) or not isinstance(reset_at, int):
        return None, None
    if not isinstance(used_percent, (int, float)):
        return None, None

    cutoff = now - timedelta(hours=6)
    points: list[tuple[datetime, float]] = []
    for sample in samples:
        observed_at = _timestamp(sample.get("observed_at"))
        sample_used = sample.get("used_percent")
        if (
            observed_at is None
            or observed_at < cutoff
            or observed_at > now
            or sample.get("limit_id") != limit_id
            or sample.get("reset_at") != reset_at
            or not isinstance(sample_used, (int, float))
        ):
            continue
        points.append((observed_at, float(sample_used)))
    points.sort(key=lambda point: point[0])
    if len(points) < 2 or points[-1][0] - points[0][0] < timedelta(minutes=30):
        return None, None

    origin = points[0][0]
    xs = [(point[0] - origin).total_seconds() / 3600.0 for point in points]
    ys = [point[1] for point in points]
    mean_x = sum(xs) / len(xs)
    mean_y = sum(ys) / len(ys)
    denominator = sum((x - mean_x) ** 2 for x in xs)
    if denominator == 0:
        return None, None
    slope = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / denominator
    slope = max(0.0, slope)
    hours_to_reset = max(0.0, (datetime.fromtimestamp(reset_at, tz=timezone.utc) - now).total_seconds() / 3600.0)
    projected = float(used_percent) + slope * hours_to_reset
    projected = min(100.0, max(float(used_percent), projected))
    return slope, int(round(projected))


def _pace(slope: float | None) -> tuple[str, str]:
    if slope is None:
        return "unknown", "UNKNOWN"
    if slope < 0.5:
        return "cool", "COOL"
    if slope < 2:
        return "normal", "NORMAL"
    if slope < 5:
        return "hot", "HOT"
    return "meltdown", "MELTDOWN"


def build_burn_rate_snapshot(
    token_events: Iterable[dict[str, Any]],
    quota_history: Iterable[dict[str, Any]],
    current_window: dict[str, Any] | None,
    now: datetime,
) -> dict[str, Any]:
    current = now if now.tzinfo is not None else now.replace(tzinfo=timezone.utc)
    current = current.astimezone(timezone.utc)
    cutoff = current - timedelta(minutes=60)
    burn_60m = 0
    for event in token_events:
        observed_at = _timestamp(event.get("timestamp"))
        total = event.get("total_tokens", event.get("total", 0))
        if observed_at is None or not isinstance(total, (int, float)):
            continue
        if cutoff < observed_at <= current:
            burn_60m += max(0, int(total))

    slope, projected = _quota_projection(quota_history, current_window, current)
    pace, pace_label = _pace(slope)
    return {
        "burn_60m": burn_60m,
        "burn_label": _compact_per_hour(burn_60m),
        "quota_points_per_hour": slope,
        "projected_used_percent": projected,
        "forecast_label": "EST --" if projected is None else f"EST {projected}%",
        "pace": pace,
        "pace_label": pace_label,
    }
