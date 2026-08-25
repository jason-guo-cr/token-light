from __future__ import annotations

import argparse
import json
import os
import sys
import time
from datetime import datetime
from pathlib import Path

from token_light.activity import read_codex_activity
from token_light.battery import read_battery_snapshot
from token_light.burn_rate import build_burn_rate_snapshot, load_quota_history
from token_light.codex_usage import UsageFetchError, UsageParseError, fetch_usage, parse_usage
from token_light.companion import build_companion_payload
from token_light.quota_history import record_quota_sample
from token_light.serial_writer import DEFAULT_PORT, detected_esp32_ports, detected_serial_ports, open_serial_port, write_snapshot
from token_light.snapshot import LOCAL_TZ, build_error_snapshot, build_snapshot, display_window
from token_light.token_usage import build_token_usage_snapshot, iter_token_count_events
from token_light.weather import WeatherFetchError, fetch_weather


def _load_mock(path: Path):
    return parse_usage(json.loads(path.read_text(encoding="utf-8")))


def _env_float(name: str) -> float | None:
    value = os.environ.get(name)
    if value is None or value == "":
        return None
    return float(value)


def _usage_from_live(codex_bin: str | None):
    return fetch_usage(codex_bin)


def _read_usage(args: argparse.Namespace):
    if args.mock:
        return _load_mock(args.mock)
    return _usage_from_live(args.codex_bin)


class UsagePoller:
    def __init__(self) -> None:
        self.usage = None
        self.error: Exception | None = None
        self.updated_at: datetime | None = None
        self.next_fetch_at = 0.0
        self.history_recorded_at: datetime | None = None

    def get(self, args: argparse.Namespace, now_monotonic: float):
        if now_monotonic >= self.next_fetch_at:
            try:
                self.usage = _read_usage(args)
                self.error = None
                self.updated_at = datetime.now(tz=LOCAL_TZ)
            except (UsageFetchError, UsageParseError) as exc:
                self.error = exc
            self.next_fetch_at = now_monotonic + args.usage_interval
        return self.usage, self.error, self.updated_at


class WeatherPoller:
    def __init__(self) -> None:
        self.weather: dict | None = None
        self.next_fetch_at = 0.0

    def get(self, args: argparse.Namespace, now_monotonic: float) -> dict | None:
        if args.weather_lat is None or args.weather_lon is None:
            return None
        if now_monotonic >= self.next_fetch_at:
            try:
                self.weather = fetch_weather(args.weather_lat, args.weather_lon, args.weather_label)
            except WeatherFetchError:
                pass
            self.next_fetch_at = now_monotonic + args.weather_interval
        return self.weather


def _build_snapshot(
    args: argparse.Namespace,
    usage_poller: UsagePoller | None = None,
    weather_poller: WeatherPoller | None = None,
    now_monotonic: float | None = None,
) -> dict:
    now = datetime.now(tz=LOCAL_TZ)
    codex_home = args.codex_home
    battery = None if args.no_battery else read_battery_snapshot()
    token_events = [] if args.no_token_usage else list(iter_token_count_events(codex_home))
    token_usage = (
        None
        if args.no_token_usage
        else build_token_usage_snapshot(codex_home=codex_home, now=now)
    )
    activity = read_codex_activity(codex_home, now)
    monotonic = time.monotonic() if now_monotonic is None else now_monotonic
    if usage_poller is None:
        usage_poller = UsagePoller()
    if weather_poller is None:
        weather_poller = WeatherPoller()
    usage, error, limit_updated_at = usage_poller.get(args, monotonic)
    weather = None if args.no_weather else weather_poller.get(args, monotonic)
    current_window = None
    remaining_percent = None
    if usage is not None:
        selected = display_window(usage)
        current_window = {
            "limit_id": selected.limit_id,
            "reset_at": selected.reset_at,
            "used_percent": selected.used_percent,
        }
        remaining_percent = selected.remaining_percent
        if (
            error is None
            and limit_updated_at is not None
            and usage_poller.history_recorded_at != limit_updated_at
            and args.mock is None
        ):
            record_quota_sample(args.quota_history, current_window, limit_updated_at)
            usage_poller.history_recorded_at = limit_updated_at

    quota_history = [] if args.mock is not None else load_quota_history(args.quota_history)
    metrics = build_burn_rate_snapshot(token_events, quota_history, current_window, now)
    if token_usage is not None:
        token_usage.update({key: metrics[key] for key in ("burn_60m", "burn_label")})
    forecast = {
        key: metrics[key]
        for key in (
            "pace",
            "pace_label",
            "quota_points_per_hour",
            "projected_used_percent",
            "forecast_label",
        )
    }
    companion = build_companion_payload(activity, remaining_percent=remaining_percent)
    quiet = (now.hour >= 22 or now.hour < 8) and not args.audio_always
    audio = {"enabled": not args.no_audio, "quiet": quiet}
    if usage is not None:
        status = "cached" if error is not None else "live"
        warning = getattr(error, "display_message", "USAGE DATA CHANGED") if error is not None else None
        return build_snapshot(
            usage,
            now=now,
            battery=battery,
            token_usage=token_usage,
            limit_updated_at=limit_updated_at,
            weather=weather,
            status=status,
            warning=warning,
            companion=companion,
            forecast=forecast,
            audio=audio,
        )
    display_message = getattr(error, "display_message", "USAGE DATA CHANGED")
    return build_error_snapshot(
        display_message,
        now=now,
        battery=battery,
        token_usage=token_usage,
        limit_updated_at=limit_updated_at,
        weather=weather,
        companion=companion,
        forecast=forecast,
        audio=audio,
    )


def run(args: argparse.Namespace) -> int:
    serial_port = None
    usage_poller = UsagePoller()
    weather_poller = WeatherPoller()

    while True:
        snapshot = _build_snapshot(args, usage_poller=usage_poller, weather_poller=weather_poller)
        if args.stdout:
            print(json.dumps(snapshot, separators=(",", ":"), ensure_ascii=True), flush=True)
        else:
            if serial_port is None:
                try:
                    serial_port = open_serial_port(args.port, settle_seconds=args.serial_settle)
                except Exception as exc:
                    print(f"Serial port unavailable: {args.port}", file=sys.stderr, flush=True)
                    print(f"Detected ESP32 ports: {', '.join(detected_esp32_ports()) or 'none'}", file=sys.stderr, flush=True)
                    print(f"Detected ports: {', '.join(detected_serial_ports()) or 'none'}", file=sys.stderr, flush=True)
                    print(str(exc), file=sys.stderr, flush=True)
                    if args.once:
                        return 2
                    time.sleep(args.interval)
                    continue
            try:
                write_snapshot(serial_port, snapshot)
            except Exception as exc:
                print(f"Serial write failed: {exc}", file=sys.stderr, flush=True)
                try:
                    serial_port.close()
                except Exception:
                    pass
                serial_port = None
                if args.once:
                    return 2
                time.sleep(args.interval)
                continue
            print(
                f"sent {snapshot.get('status', 'unknown')} snapshot at {snapshot.get('time', '--:--')}",
                file=sys.stderr,
                flush=True,
            )
        if args.once:
            return 0
        time.sleep(args.interval)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Sync Codex account usage snapshots to Token Light.")
    parser.add_argument("--codex-bin", default=os.environ.get("TOKEN_LIGHT_CODEX_BIN"))
    parser.add_argument("--port", default=os.environ.get("TOKEN_LIGHT_PORT", DEFAULT_PORT))
    parser.add_argument("--interval", type=int, default=60)
    parser.add_argument("--usage-interval", type=int, default=600)
    parser.add_argument("--weather-interval", type=int, default=1800)
    parser.add_argument("--weather-lat", type=float, default=_env_float("TOKEN_LIGHT_WEATHER_LAT"))
    parser.add_argument("--weather-lon", type=float, default=_env_float("TOKEN_LIGHT_WEATHER_LON"))
    parser.add_argument("--weather-label", default=os.environ.get("TOKEN_LIGHT_WEATHER_LABEL", "BJ"))
    parser.add_argument("--serial-settle", type=float, default=3.0)
    parser.add_argument("--codex-home", type=Path, default=Path.home() / ".codex")
    parser.add_argument(
        "--quota-history",
        type=Path,
        default=Path.home() / ".cache" / "token-light" / "quota-history.jsonl",
    )
    parser.add_argument("--audio-always", action="store_true")
    parser.add_argument("--no-audio", action="store_true")
    parser.add_argument("--no-battery", action="store_true")
    parser.add_argument("--no-token-usage", action="store_true")
    parser.add_argument("--no-weather", action="store_true")
    parser.add_argument("--mock", type=Path)
    parser.add_argument("--stdout", action="store_true")
    parser.add_argument("--once", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    return run(build_parser().parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
