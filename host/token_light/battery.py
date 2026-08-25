from __future__ import annotations

import re
import subprocess


BATTERY_RE = re.compile(r"(?P<percent>\d+)%;\s*(?P<state>[^;]+);")


def parse_pmset_battery(output: str) -> dict | None:
    match = BATTERY_RE.search(output)
    if not match:
        return None

    percent = max(0, min(100, int(match.group("percent"))))
    state = match.group("state").strip().lower()
    return {
        "percent": percent,
        "charging": "charging" in state and "discharging" not in state,
        "state": state,
    }


def read_battery_snapshot() -> dict | None:
    try:
        result = subprocess.run(
            ["pmset", "-g", "batt"],
            check=False,
            capture_output=True,
            text=True,
            timeout=3,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None

    if result.returncode != 0:
        return None
    return parse_pmset_battery(result.stdout)
