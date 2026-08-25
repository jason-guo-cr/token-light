from __future__ import annotations

import json
import os
import selectors
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


APP_SERVER_REQUEST_ID = 2


class UsageParseError(RuntimeError):
    pass


class UsageFetchError(RuntimeError):
    def __init__(self, message: str, display_message: str = "SYNC UNAVAILABLE") -> None:
        super().__init__(message)
        self.display_message = display_message


@dataclass(frozen=True)
class UsageWindow:
    used_percent: int
    remaining_percent: int
    window_minutes: int
    reset_at: int
    limit_id: str = "codex"
    limit_name: str | None = None


@dataclass(frozen=True)
class UsageStatus:
    plan_type: str | None
    windows: tuple[UsageWindow, ...]

    @property
    def primary(self) -> UsageWindow:
        return self.windows[0]

    @property
    def secondary(self) -> UsageWindow | None:
        return self.windows[1] if len(self.windows) > 1 else None


def _clamp_percent(value: int) -> int:
    return min(100, max(0, value))


def _read_window(
    window: Any,
    *,
    limit_id: str,
    limit_name: str | None,
    path: str,
    app_server_shape: bool,
) -> UsageWindow | None:
    if window is None:
        return None
    if not isinstance(window, dict):
        raise UsageParseError(f"{path} is invalid")

    try:
        if app_server_shape:
            used_percent = int(window["usedPercent"])
            window_minutes = int(window["windowDurationMins"])
            reset_at = int(window["resetsAt"])
        else:
            used_percent = int(window["used_percent"])
            window_minutes = int(window["limit_window_seconds"]) // 60
            reset_at = int(window["reset_at"])
    except KeyError as exc:
        raise UsageParseError(f"{path}.{exc.args[0]} is missing") from exc
    except (TypeError, ValueError) as exc:
        raise UsageParseError(f"{path} contains invalid values") from exc

    return UsageWindow(
        used_percent=used_percent,
        remaining_percent=_clamp_percent(100 - used_percent),
        window_minutes=window_minutes,
        reset_at=reset_at,
        limit_id=limit_id,
        limit_name=limit_name,
    )


def _read_app_server_bucket(bucket: Any, fallback_id: str) -> tuple[str | None, list[UsageWindow]]:
    if not isinstance(bucket, dict):
        raise UsageParseError(f"rateLimitsByLimitId.{fallback_id} is invalid")

    limit_id = str(bucket.get("limitId") or fallback_id)
    raw_name = bucket.get("limitName")
    limit_name = str(raw_name) if raw_name is not None else None
    raw_plan = bucket.get("planType")
    plan_type = str(raw_plan) if raw_plan is not None else None
    windows: list[UsageWindow] = []
    for key in ("primary", "secondary"):
        parsed = _read_window(
            bucket.get(key),
            limit_id=limit_id,
            limit_name=limit_name,
            path=f"{limit_id}.{key}",
            app_server_shape=True,
        )
        if parsed is not None:
            windows.append(parsed)
    return plan_type, windows


def _parse_app_server_usage(payload: dict[str, Any]) -> UsageStatus:
    result = payload.get("result", payload)
    if not isinstance(result, dict):
        raise UsageParseError("app-server result is missing or invalid")

    primary_bucket = result.get("rateLimits")
    buckets_by_id = result.get("rateLimitsByLimitId")
    ordered_buckets: list[tuple[str, Any]] = []
    seen: set[str] = set()

    if isinstance(primary_bucket, dict):
        primary_id = str(primary_bucket.get("limitId") or "codex")
        ordered_buckets.append((primary_id, primary_bucket))
        seen.add(primary_id)

    if isinstance(buckets_by_id, dict):
        for fallback_id, bucket in buckets_by_id.items():
            bucket_id = str(bucket.get("limitId") or fallback_id) if isinstance(bucket, dict) else str(fallback_id)
            if bucket_id not in seen:
                ordered_buckets.append((str(fallback_id), bucket))
                seen.add(bucket_id)

    if not ordered_buckets:
        raise UsageParseError("rateLimits is missing or invalid")

    plan_type: str | None = None
    windows: list[UsageWindow] = []
    for fallback_id, bucket in ordered_buckets:
        bucket_plan, bucket_windows = _read_app_server_bucket(bucket, fallback_id)
        plan_type = plan_type or bucket_plan
        windows.extend(bucket_windows)

    if not windows:
        raise UsageParseError("rateLimits contains no quota windows")
    return UsageStatus(plan_type=plan_type, windows=tuple(windows))


def _parse_legacy_usage(payload: dict[str, Any]) -> UsageStatus:
    rate_limit = payload.get("rate_limit")
    if not isinstance(rate_limit, dict):
        raise UsageParseError("rate_limit is missing or invalid")

    raw_plan = rate_limit.get("plan_type", payload.get("plan_type"))
    plan_type = str(raw_plan) if raw_plan is not None else None
    windows: list[UsageWindow] = []
    for key in ("primary_window", "secondary_window"):
        parsed = _read_window(
            rate_limit.get(key),
            limit_id="codex",
            limit_name=None,
            path=f"rate_limit.{key}",
            app_server_shape=False,
        )
        if parsed is not None:
            windows.append(parsed)

    additional = payload.get("additional_rate_limits", [])
    if isinstance(additional, list):
        for index, item in enumerate(additional):
            if not isinstance(item, dict) or not isinstance(item.get("rate_limit"), dict):
                continue
            limit_id = str(item.get("metered_feature") or f"additional_{index}")
            raw_name = item.get("limit_name")
            limit_name = str(raw_name) if raw_name is not None else None
            bucket = item["rate_limit"]
            for key in ("primary_window", "secondary_window"):
                parsed = _read_window(
                    bucket.get(key),
                    limit_id=limit_id,
                    limit_name=limit_name,
                    path=f"additional_rate_limits.{index}.{key}",
                    app_server_shape=False,
                )
                if parsed is not None:
                    windows.append(parsed)

    if not windows:
        raise UsageParseError("rate_limit contains no quota windows")
    return UsageStatus(plan_type=plan_type, windows=tuple(windows))


def parse_usage(payload: Any) -> UsageStatus:
    if not isinstance(payload, dict):
        raise UsageParseError("usage response is not a JSON object")
    if "rateLimits" in payload or "rateLimitsByLimitId" in payload or isinstance(payload.get("result"), dict):
        return _parse_app_server_usage(payload)
    return _parse_legacy_usage(payload)


def resolve_codex_binary(configured: str | None = None) -> str:
    candidate = configured or os.environ.get("TOKEN_LIGHT_CODEX_BIN")
    if candidate:
        return str(Path(candidate).expanduser())

    discovered = shutil.which("codex")
    if discovered:
        return discovered

    for path in ("/opt/homebrew/bin/codex", "/usr/local/bin/codex"):
        if Path(path).is_file():
            return path
    raise UsageFetchError("Codex CLI was not found", "CODEX CLI NOT FOUND")


def _app_server_input() -> str:
    messages = (
        {
            "method": "initialize",
            "id": 1,
            "params": {
                "clientInfo": {
                    "name": "token_light",
                    "title": "Token Light",
                    "version": "0.1.0",
                }
            },
        },
        {"method": "initialized", "params": {}},
        {"method": "account/rateLimits/read", "id": APP_SERVER_REQUEST_ID},
    )
    return "".join(json.dumps(message, separators=(",", ":")) + "\n" for message in messages)


def _read_app_server_line(line: str) -> UsageStatus | None:
    try:
        message = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(message, dict) or message.get("id") != APP_SERVER_REQUEST_ID:
        return None
    error = message.get("error")
    if isinstance(error, dict):
        detail = str(error.get("message") or "unknown app-server error")
        raise UsageFetchError(f"Codex app-server rejected rate-limit request: {detail}", "SIGN IN TO CODEX")
    return parse_usage(message)


def fetch_usage(codex_bin: str | None = None, timeout: float = 10.0) -> UsageStatus:
    executable = resolve_codex_binary(codex_bin)
    process: subprocess.Popen[str] | None = None
    try:
        process = subprocess.Popen(
            [executable, "app-server"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        assert process.stdin is not None
        assert process.stdout is not None
        process.stdin.write(_app_server_input())
        process.stdin.flush()

        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            if not selector.select(remaining):
                break
            line = process.stdout.readline()
            if not line:
                break
            usage = _read_app_server_line(line)
            if usage is not None:
                return usage

        stderr = ""
        if process.poll() is not None and process.stderr is not None:
            stderr = process.stderr.read().strip()
        if process.returncode not in (None, 0) and stderr:
            raise UsageFetchError(
                f"Codex app-server exited with {process.returncode}: {stderr}",
                "CODEX SYNC FAILED",
            )
        raise UsageFetchError("Codex app-server returned no rate-limit response", "NO USAGE DATA")
    except FileNotFoundError as exc:
        raise UsageFetchError(f"Codex CLI was not found: {executable}", "CODEX CLI NOT FOUND") from exc
    except OSError as exc:
        raise UsageFetchError(f"Failed to start Codex app-server: {exc}") from exc
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
