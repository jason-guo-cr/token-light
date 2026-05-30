from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

from token_light.auth import DEFAULT_AUTH_FILE, AuthError, read_access_token
from token_light.codex_usage import UsageFetchError, UsageParseError, fetch_usage, parse_usage
from token_light.serial_writer import DEFAULT_PORT, detected_serial_ports, open_serial_port, write_snapshot
from token_light.snapshot import build_error_snapshot, build_snapshot


def _load_mock(path: Path):
    return parse_usage(json.loads(path.read_text(encoding="utf-8")))


def _snapshot_from_live(auth_file: Path) -> dict:
    token = read_access_token(auth_file)
    return build_snapshot(fetch_usage(token))


def _build_snapshot(args: argparse.Namespace) -> dict:
    if args.mock:
        return build_snapshot(_load_mock(args.mock))
    try:
        return _snapshot_from_live(args.auth_file)
    except (AuthError, UsageFetchError, UsageParseError) as exc:
        return build_error_snapshot(str(exc))


def run(args: argparse.Namespace) -> int:
    serial_port = None
    if not args.stdout:
        try:
            serial_port = open_serial_port(args.port)
        except Exception as exc:
            print(f"Serial port unavailable: {args.port}", file=sys.stderr)
            print(f"Detected ports: {', '.join(detected_serial_ports()) or 'none'}", file=sys.stderr)
            print(str(exc), file=sys.stderr)
            return 2

    while True:
        snapshot = _build_snapshot(args)
        if args.stdout:
            print(json.dumps(snapshot, separators=(",", ":"), ensure_ascii=True), flush=True)
        else:
            write_snapshot(serial_port, snapshot)
        if args.once:
            return 0
        time.sleep(args.interval)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Sync Codex usage-limit snapshots to Token Light.")
    parser.add_argument("--auth-file", type=Path, default=Path(os.environ.get("TOKEN_LIGHT_AUTH_FILE", DEFAULT_AUTH_FILE)))
    parser.add_argument("--port", default=os.environ.get("TOKEN_LIGHT_PORT", DEFAULT_PORT))
    parser.add_argument("--interval", type=int, default=60)
    parser.add_argument("--mock", type=Path)
    parser.add_argument("--stdout", action="store_true")
    parser.add_argument("--once", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    return run(build_parser().parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
