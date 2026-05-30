from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

from token_light.auth import DEFAULT_AUTH_FILE, AuthError, read_access_token
from token_light.battery import read_battery_snapshot
from token_light.codex_usage import UsageFetchError, UsageParseError, fetch_usage, parse_usage
from token_light.serial_writer import DEFAULT_PORT, detected_esp32_ports, detected_serial_ports, open_serial_port, write_snapshot
from token_light.snapshot import build_error_snapshot, build_snapshot
from token_light.token_usage import build_token_usage_snapshot


def _load_mock(path: Path):
    return parse_usage(json.loads(path.read_text(encoding="utf-8")))


def _snapshot_from_live(auth_file: Path, battery: dict | None, token_usage: dict | None) -> dict:
    token = read_access_token(auth_file)
    return build_snapshot(fetch_usage(token), battery=battery, token_usage=token_usage)


def _build_snapshot(args: argparse.Namespace) -> dict:
    battery = None if args.no_battery else read_battery_snapshot()
    token_usage = None if args.no_token_usage else build_token_usage_snapshot()
    if args.mock:
        return build_snapshot(_load_mock(args.mock), battery=battery, token_usage=token_usage)
    try:
        return _snapshot_from_live(args.auth_file, battery, token_usage)
    except (AuthError, UsageFetchError, UsageParseError) as exc:
        return build_error_snapshot(str(exc), battery=battery, token_usage=token_usage)


def run(args: argparse.Namespace) -> int:
    serial_port = None

    while True:
        snapshot = _build_snapshot(args)
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
    parser = argparse.ArgumentParser(description="Sync Codex usage-limit snapshots to Token Light.")
    parser.add_argument("--auth-file", type=Path, default=Path(os.environ.get("TOKEN_LIGHT_AUTH_FILE", DEFAULT_AUTH_FILE)))
    parser.add_argument("--port", default=os.environ.get("TOKEN_LIGHT_PORT", DEFAULT_PORT))
    parser.add_argument("--interval", type=int, default=60)
    parser.add_argument("--serial-settle", type=float, default=3.0)
    parser.add_argument("--no-battery", action="store_true")
    parser.add_argument("--no-token-usage", action="store_true")
    parser.add_argument("--mock", type=Path)
    parser.add_argument("--stdout", action="store_true")
    parser.add_argument("--once", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    return run(build_parser().parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
