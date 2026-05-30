from __future__ import annotations

import json
import socket
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any


USAGE_URL = "https://chatgpt.com/backend-api/wham/usage"


class UsageParseError(RuntimeError):
    pass


class UsageFetchError(RuntimeError):
    pass


@dataclass(frozen=True)
class UsageWindow:
    used_percent: int
    remaining_percent: int
    window_minutes: int
    reset_at: int


@dataclass(frozen=True)
class UsageStatus:
    plan_type: str | None
    primary: UsageWindow
    secondary: UsageWindow


def _clamp_percent(value: int) -> int:
    return min(100, max(0, value))


def _read_window(rate_limit: dict[str, Any], key: str) -> UsageWindow:
    window = rate_limit.get(key)
    if not isinstance(window, dict):
        raise UsageParseError(f"rate_limit.{key} is missing or invalid")

    try:
        used_percent = int(window["used_percent"])
        limit_window_seconds = int(window["limit_window_seconds"])
        reset_at = int(window["reset_at"])
    except KeyError as exc:
        raise UsageParseError(f"rate_limit.{key}.{exc.args[0]} is missing") from exc
    except (TypeError, ValueError) as exc:
        raise UsageParseError(f"rate_limit.{key} contains invalid values") from exc

    return UsageWindow(
        used_percent=used_percent,
        remaining_percent=_clamp_percent(100 - used_percent),
        window_minutes=limit_window_seconds // 60,
        reset_at=reset_at,
    )


def parse_usage(payload: Any) -> UsageStatus:
    if not isinstance(payload, dict):
        raise UsageParseError("usage response is not a JSON object")

    rate_limit = payload.get("rate_limit")
    if not isinstance(rate_limit, dict):
        raise UsageParseError("rate_limit is missing or invalid")

    plan_type = rate_limit.get("plan_type")
    if plan_type is not None and not isinstance(plan_type, str):
        plan_type = str(plan_type)

    return UsageStatus(
        plan_type=plan_type,
        primary=_read_window(rate_limit, "primary_window"),
        secondary=_read_window(rate_limit, "secondary_window"),
    )


def fetch_usage(access_token: str, timeout: float = 10.0) -> UsageStatus:
    request = urllib.request.Request(
        USAGE_URL,
        headers={
            "Authorization": f"Bearer {access_token}",
            "Accept": "application/json",
            "User-Agent": "token-light/0.1",
        },
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = json.loads(response.read())
    except (urllib.error.URLError, TimeoutError, socket.timeout) as exc:
        raise UsageFetchError(f"Failed to fetch Codex usage: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise UsageFetchError("Codex usage response is not valid JSON") from exc

    return parse_usage(payload)
