from __future__ import annotations

import json
import re
from datetime import date, datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo


LOCAL_TZ = ZoneInfo("Asia/Shanghai")
SESSION_RE = re.compile(r"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})")


def _session_id(path: Path) -> str:
    match = SESSION_RE.search(path.name)
    return match.group(1) if match else path.stem


def _iter_token_events(codex_home: Path, tz: ZoneInfo):
    seen = set()
    for root_name in ("sessions", "archived_sessions"):
        root = codex_home / root_name
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
                    payload = obj.get("payload") or {}
                    if payload.get("type") != "token_count":
                        continue
                    usage = ((payload.get("info") or {}).get("last_token_usage") or {})
                    timestamp = obj.get("timestamp")
                    if not timestamp or not usage:
                        continue
                    key = (
                        sid,
                        timestamp,
                        usage.get("total_tokens"),
                        usage.get("input_tokens"),
                        usage.get("output_tokens"),
                    )
                    if key in seen:
                        continue
                    seen.add(key)
                    try:
                        event_time = datetime.fromisoformat(timestamp.replace("Z", "+00:00")).astimezone(tz)
                    except ValueError:
                        continue
                    yield {
                        "date": event_time.date(),
                        "total": usage.get("total_tokens") or 0,
                        "input": usage.get("input_tokens") or 0,
                        "cached_input": usage.get("cached_input_tokens") or 0,
                        "output": usage.get("output_tokens") or 0,
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
