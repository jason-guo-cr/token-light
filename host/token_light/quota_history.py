from __future__ import annotations

import json
import os
from datetime import datetime, timedelta, timezone
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any

from token_light.burn_rate import load_quota_history


def _as_utc(value: datetime) -> datetime:
    if value.tzinfo is None:
        value = value.replace(tzinfo=timezone.utc)
    return value.astimezone(timezone.utc)


def record_quota_sample(
    path: Path | str,
    window: dict[str, Any],
    observed_at: datetime,
    *,
    retention_days: int = 14,
) -> None:
    history_path = Path(path).expanduser()
    observed = _as_utc(observed_at)
    sample = {
        "observed_at": observed.isoformat().replace("+00:00", "Z"),
        "limit_id": str(window["limit_id"]),
        "reset_at": int(window["reset_at"]),
        "used_percent": float(window["used_percent"]),
    }
    cutoff = observed - timedelta(days=retention_days)
    retained = []
    for existing in load_quota_history(history_path):
        try:
            existing_at = datetime.fromisoformat(str(existing["observed_at"]).replace("Z", "+00:00"))
        except (KeyError, ValueError):
            continue
        if _as_utc(existing_at) >= cutoff:
            retained.append(existing)
    retained.append(sample)

    history_path.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(
        "w", encoding="utf-8", dir=history_path.parent, prefix=f".{history_path.name}.", delete=False
    ) as handle:
        temporary = Path(handle.name)
        for item in retained:
            handle.write(json.dumps(item, separators=(",", ":"), ensure_ascii=True) + "\n")
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temporary, history_path)
