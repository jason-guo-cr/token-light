from __future__ import annotations

import json
import math
import re
from datetime import date, datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo


LOCAL_TZ = ZoneInfo("Asia/Shanghai")
SESSION_RE = re.compile(r"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})")


def _session_id(path: Path) -> str:
    match = SESSION_RE.search(path.name)
    return match.group(1) if match else path.stem


def _token_value(value, *, default=None) -> int | None:
    if value is None:
        return default
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    if not math.isfinite(value) or value < 0:
        return None
    return int(value)


def iter_token_count_events(codex_home: Path | str | None = None):
    home = Path(codex_home).expanduser() if codex_home is not None else Path.home() / ".codex"
    seen = set()
    for root_name in ("sessions", "archived_sessions"):
        root = home / root_name
        if not root.exists():
            continue
        for path in root.rglob("*.jsonl"):
            sid = _session_id(path)
            try:
                lines = path.open("r", encoding="utf-8", errors="replace")
            except OSError:
                continue
            with lines:
                for line in lines:
                    if '"token_count"' not in line:
                        continue
                    try:
                        obj = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if not isinstance(obj, dict):
                        continue
                    payload = obj.get("payload")
                    if not isinstance(payload, dict):
                        continue
                    if payload.get("type") != "token_count":
                        continue
                    info = payload.get("info")
                    if not isinstance(info, dict):
                        continue
                    usage = info.get("last_token_usage")
                    if not isinstance(usage, dict):
                        continue
                    timestamp = obj.get("timestamp")
                    if not timestamp:
                        continue
                    total = _token_value(usage.get("total_tokens"))
                    input_tokens = _token_value(usage.get("input_tokens"), default=0)
                    cached_input = _token_value(usage.get("cached_input_tokens"), default=0)
                    output = _token_value(usage.get("output_tokens"), default=0)
                    if None in (total, input_tokens, cached_input, output):
                        continue
                    key = (
                        sid,
                        timestamp,
                        total,
                        input_tokens,
                        cached_input,
                        output,
                    )
                    if key in seen:
                        continue
                    seen.add(key)
                    try:
                        datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
                    except (TypeError, ValueError):
                        continue
                    yield {
                        "timestamp": timestamp,
                        "total_tokens": total,
                        "input": input_tokens,
                        "cached_input": cached_input,
                        "output": output,
                    }


def _iter_token_events(codex_home: Path, tz: ZoneInfo):
    for event in iter_token_count_events(codex_home):
        try:
            event_time = datetime.fromisoformat(event["timestamp"].replace("Z", "+00:00")).astimezone(tz)
        except (TypeError, ValueError):
            continue
        yield {
            "date": event_time.date(),
            "total": event["total_tokens"],
            "input": event["input"],
            "cached_input": event["cached_input"],
            "output": event["output"],
        }


def _format_compact(value: int) -> str:
    if value >= 1_000_000_000:
        return f"{value / 1_000_000_000:.1f}B"
    if value >= 1_000_000:
        return f"{value / 1_000_000:.1f}M"
    if value >= 10_000:
        return f"{value / 1_000:.0f}K"
    if value >= 1_000:
        return f"{value / 1_000:.1f}K"
    return str(value)


def build_token_usage_snapshot(
    codex_home: Path | str | None = None,
    now: datetime | None = None,
    tz: ZoneInfo = LOCAL_TZ,
) -> dict:
    home = Path(codex_home).expanduser() if codex_home is not None else Path.home() / ".codex"
    current_date = (now or datetime.now(tz)).astimezone(tz).date()
    week_start = current_date - timedelta(days=current_date.weekday())
    today_total = 0
    week_total = 0

    for event in _iter_token_events(home, tz):
        event_date: date = event["date"]
        total = int(event["total"])
        if event_date == current_date:
            today_total += total
        if week_start <= event_date <= current_date:
            week_total += total

    return {
        "today_total": today_total,
        "week_total": week_total,
        "today_label": _format_compact(today_total),
        "week_label": _format_compact(week_total),
    }
