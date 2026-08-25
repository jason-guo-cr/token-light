# Token Light Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and flash a polished Waveshare ESP32-S3-RLCD-4.2 dashboard that shows Codex usage-limit remaining percentages, reset labels, and local date/time.

**Architecture:** A Mac host sync agent reads the existing Codex Desktop auth token, calls the same ChatGPT usage endpoint the app uses, converts the response into compact snapshot JSON, and sends it over USB serial. The ESP32 firmware never stores credentials; it receives newline-delimited snapshots, keeps local display state, and renders a monochrome 400 x 300 U8g2 dashboard using Waveshare's ST7305 driver.

**Tech Stack:** Python 3.11 standard library, `pyserial`, `unittest`, PlatformIO, Arduino for ESP32-S3, ArduinoJson, U8g2, Waveshare `ESP32-S3-RLCD-4.2` driver files pinned to `cb590c853f3d2bb58efe280c98b2f3cbf1e2876e`.

---

## File Structure

- Create `.gitignore` to keep virtual environments, build output, and local vendor clones out of git.
- Create `pyproject.toml` so tests and CLI can import the host package from `host/`.
- Create `requirements-dev.txt` with `pyserial` and `platformio`.
- Create `host/token_light/__init__.py` for package metadata.
- Create `host/token_light/auth.py` to read `tokens.access_token` from `~/.codex/auth.json`.
- Create `host/token_light/codex_usage.py` to fetch and parse `https://chatgpt.com/backend-api/wham/usage`.
- Create `host/token_light/snapshot.py` to format date/time, window labels, remaining percentages, and error snapshots.
- Create `host/token_light/serial_writer.py` to list ports and write one compact JSON line per snapshot.
- Create `host/token_light/cli.py` to run once, run every minute, print mock snapshots, and stream to the board.
- Create `tests/fixtures/wham_usage.json` with a sanitized sample response matching the locally verified endpoint shape.
- Create `tests/test_auth.py`, `tests/test_codex_usage.py`, `tests/test_snapshot.py`, `tests/test_cli.py`, and `tests/test_serial_writer.py`.
- Create `firmware/platformio.ini` for ESP32-S3 build/upload.
- Create `firmware/src/ST7305_U8g2.h` and `firmware/src/ST7305_U8g2.cpp` copied from Waveshare's `02_Example/Arduino/10_U8G2_Test/` at commit `cb590c853f3d2bb58efe280c98b2f3cbf1e2876e`.
- Create `firmware/src/dashboard_state.h` for parsed display state structs.
- Create `firmware/src/dashboard_render.h` and `firmware/src/dashboard_render.cpp` for fixed-dimension U8g2 drawing.
- Create `firmware/src/main.cpp` for display setup, serial line buffering, JSON parsing, stale detection, and render loop.
- Create `scripts/bootstrap_waveshare_driver.sh` to copy the two ST7305 driver files from a temporary clone and print the pinned source commit.

## Task 1: Host Project Scaffold And Codex Auth Reader

**Files:**
- Create: `.gitignore`
- Create: `pyproject.toml`
- Create: `requirements-dev.txt`
- Create: `host/token_light/__init__.py`
- Create: `host/token_light/auth.py`
- Test: `tests/test_auth.py`

- [ ] **Step 1: Write the failing auth tests**

Create `tests/test_auth.py`:

```python
import json
import tempfile
import unittest
from pathlib import Path

from token_light.auth import AuthError, read_access_token


class AuthTests(unittest.TestCase):
    def test_reads_access_token_from_codex_auth_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            auth_path = Path(tmp) / "auth.json"
            auth_path.write_text(
                json.dumps({"tokens": {"access_token": "secret-token"}}),
                encoding="utf-8",
            )

            self.assertEqual(read_access_token(auth_path), "secret-token")

    def test_missing_token_raises_clear_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            auth_path = Path(tmp) / "auth.json"
            auth_path.write_text(json.dumps({"tokens": {}}), encoding="utf-8")

            with self.assertRaisesRegex(AuthError, "access token"):
                read_access_token(auth_path)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the auth tests and verify they fail**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_auth -v
```

Expected: `ModuleNotFoundError: No module named 'token_light'`.

- [ ] **Step 3: Add the package scaffold and auth implementation**

Create `.gitignore`:

```gitignore
.venv/
__pycache__/
*.pyc
.pytest_cache/
.mypy_cache/
firmware/.pio/
firmware/.vscode/
vendor/
```

Create `pyproject.toml`:

```toml
[build-system]
requires = ["setuptools>=69"]
build-backend = "setuptools.build_meta"

[project]
name = "token-light"
version = "0.1.0"
requires-python = ">=3.11"
dependencies = ["pyserial>=3.5"]

[project.scripts]
token-light = "token_light.cli:main"

[tool.setuptools]
package-dir = {"" = "host"}

[tool.setuptools.packages.find]
where = ["host"]
```

Create `requirements-dev.txt`:

```text
-e .
pyserial>=3.5
platformio>=6.1
```

Create `host/token_light/__init__.py`:

```python
__version__ = "0.1.0"
```

Create `host/token_light/auth.py`:

```python
from __future__ import annotations

import json
from pathlib import Path


DEFAULT_AUTH_FILE = Path.home() / ".codex" / "auth.json"


class AuthError(RuntimeError):
    pass


def read_access_token(auth_file: Path | str = DEFAULT_AUTH_FILE) -> str:
    path = Path(auth_file).expanduser()
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise AuthError(f"Codex auth file not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise AuthError(f"Codex auth file is not valid JSON: {path}") from exc

    token = payload.get("tokens", {}).get("access_token")
    if not isinstance(token, str) or not token:
        raise AuthError(f"Codex auth file does not contain an access token: {path}")
    return token
```

- [ ] **Step 4: Run the auth tests and verify they pass**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_auth -v
```

Expected: `OK`.

- [ ] **Step 5: Commit Task 1**

Run:

```bash
git add .gitignore pyproject.toml requirements-dev.txt host/token_light/__init__.py host/token_light/auth.py tests/test_auth.py
git commit -m "feat: add host auth reader"
```

## Task 2: Codex Usage Response Parser

**Files:**
- Create: `host/token_light/codex_usage.py`
- Create: `tests/fixtures/wham_usage.json`
- Test: `tests/test_codex_usage.py`

- [ ] **Step 1: Write the failing parser fixture and tests**

Create `tests/fixtures/wham_usage.json`:

```json
{
  "rate_limit": {
    "plan_type": "prolite",
    "primary_window": {
      "used_percent": 3,
      "limit_window_seconds": 18000,
      "reset_at": 1780144603
    },
    "secondary_window": {
      "used_percent": 4,
      "limit_window_seconds": 604800,
      "reset_at": 1780217709
    }
  }
}
```

Create `tests/test_codex_usage.py`:

```python
import json
import unittest
from pathlib import Path

from token_light.codex_usage import UsageParseError, parse_usage


FIXTURE = Path(__file__).parent / "fixtures" / "wham_usage.json"


class CodexUsageTests(unittest.TestCase):
    def test_parse_usage_extracts_primary_and_secondary_windows(self):
        payload = json.loads(FIXTURE.read_text(encoding="utf-8"))

        usage = parse_usage(payload)

        self.assertEqual(usage.plan_type, "prolite")
        self.assertEqual(usage.primary.used_percent, 3)
        self.assertEqual(usage.primary.remaining_percent, 97)
        self.assertEqual(usage.primary.window_minutes, 300)
        self.assertEqual(usage.primary.reset_at, 1780144603)
        self.assertEqual(usage.secondary.used_percent, 4)
        self.assertEqual(usage.secondary.remaining_percent, 96)
        self.assertEqual(usage.secondary.window_minutes, 10080)

    def test_remaining_percent_is_clamped(self):
        payload = {
            "rate_limit": {
                "primary_window": {
                    "used_percent": -5,
                    "limit_window_seconds": 18000,
                    "reset_at": 1780144603,
                },
                "secondary_window": {
                    "used_percent": 125,
                    "limit_window_seconds": 604800,
                    "reset_at": 1780217709,
                },
            }
        }

        usage = parse_usage(payload)

        self.assertEqual(usage.primary.remaining_percent, 100)
        self.assertEqual(usage.secondary.remaining_percent, 0)

    def test_missing_rate_limit_raises_parse_error(self):
        with self.assertRaisesRegex(UsageParseError, "rate_limit"):
            parse_usage({})


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the parser tests and verify they fail**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_codex_usage -v
```

Expected: `ModuleNotFoundError` or `ImportError` for `token_light.codex_usage`.

- [ ] **Step 3: Implement the parser**

Create `host/token_light/codex_usage.py`:

```python
from __future__ import annotations

import json
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
    return max(0, min(100, value))


def _read_window(container: dict[str, Any], key: str) -> UsageWindow:
    raw = container.get(key)
    if not isinstance(raw, dict):
        raise UsageParseError(f"rate_limit.{key} is missing")
    try:
        used_percent = int(raw["used_percent"])
        window_minutes = int(raw["limit_window_seconds"]) // 60
        reset_at = int(raw["reset_at"])
    except (KeyError, TypeError, ValueError) as exc:
        raise UsageParseError(f"rate_limit.{key} has invalid fields") from exc
    return UsageWindow(
        used_percent=_clamp_percent(used_percent),
        remaining_percent=_clamp_percent(100 - used_percent),
        window_minutes=window_minutes,
        reset_at=reset_at,
    )


def parse_usage(payload: dict[str, Any]) -> UsageStatus:
    rate_limit = payload.get("rate_limit")
    if not isinstance(rate_limit, dict):
        raise UsageParseError("rate_limit is missing")
    return UsageStatus(
        plan_type=rate_limit.get("plan_type"),
        primary=_read_window(rate_limit, "primary_window"),
        secondary=_read_window(rate_limit, "secondary_window"),
    )


def fetch_usage(access_token: str, timeout_seconds: float = 20.0) -> UsageStatus:
    request = urllib.request.Request(
        USAGE_URL,
        headers={
            "Authorization": f"Bearer {access_token}",
            "Accept": "application/json",
            "User-Agent": "token-light/0.1",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise UsageFetchError("Codex usage request failed") from exc
    return parse_usage(payload)
```

- [ ] **Step 4: Run the parser tests and verify they pass**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_codex_usage -v
```

Expected: `OK`.

- [ ] **Step 5: Commit Task 2**

Run:

```bash
git add host/token_light/codex_usage.py tests/fixtures/wham_usage.json tests/test_codex_usage.py
git commit -m "feat: parse codex usage limits"
```

## Task 3: Snapshot Builder And Reset Labels

**Files:**
- Create: `host/token_light/snapshot.py`
- Test: `tests/test_snapshot.py`

- [ ] **Step 1: Write the failing snapshot tests**

Create `tests/test_snapshot.py`:

```python
import unittest
from datetime import datetime
from zoneinfo import ZoneInfo

from token_light.codex_usage import UsageStatus, UsageWindow
from token_light.snapshot import build_error_snapshot, build_snapshot


class SnapshotTests(unittest.TestCase):
    def test_build_snapshot_formats_usage_windows_for_display(self):
        usage = UsageStatus(
            plan_type="prolite",
            primary=UsageWindow(used_percent=3, remaining_percent=97, window_minutes=300, reset_at=1780144603),
            secondary=UsageWindow(used_percent=4, remaining_percent=96, window_minutes=10080, reset_at=1780217709),
        )
        now = datetime(2026, 5, 30, 15, 50, tzinfo=ZoneInfo("Asia/Shanghai"))

        snapshot = build_snapshot(usage, now=now)

        self.assertEqual(snapshot["type"], "snapshot")
        self.assertEqual(snapshot["date"], "05/30")
        self.assertEqual(snapshot["weekday"], "SAT")
        self.assertEqual(snapshot["time"], "15:50")
        self.assertEqual(snapshot["primary"]["label"], "5H LIMIT")
        self.assertEqual(snapshot["primary"]["remaining_percent"], 97)
        self.assertEqual(snapshot["primary"]["reset_label"], "20:36")
        self.assertEqual(snapshot["secondary"]["label"], "WEEK LIMIT")
        self.assertEqual(snapshot["secondary"]["remaining_percent"], 96)
        self.assertEqual(snapshot["secondary"]["reset_label"], "05/31")
        self.assertEqual(snapshot["status"], "live")

    def test_error_snapshot_keeps_time_and_message(self):
        now = datetime(2026, 5, 30, 15, 50, tzinfo=ZoneInfo("Asia/Shanghai"))

        snapshot = build_error_snapshot("Codex usage request failed", now=now)

        self.assertEqual(snapshot["status"], "api_error")
        self.assertEqual(snapshot["message"], "Codex usage request failed")
        self.assertEqual(snapshot["time"], "15:50")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the snapshot tests and verify they fail**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_snapshot -v
```

Expected: `ModuleNotFoundError` or `ImportError` for `token_light.snapshot`.

- [ ] **Step 3: Implement snapshot formatting**

Create `host/token_light/snapshot.py`:

```python
from __future__ import annotations

from datetime import datetime
from zoneinfo import ZoneInfo

from token_light.codex_usage import UsageStatus, UsageWindow


LOCAL_TZ = ZoneInfo("Asia/Shanghai")


def _now() -> datetime:
    return datetime.now(tz=LOCAL_TZ)


def _base_snapshot(now: datetime) -> dict:
    local = now.astimezone(LOCAL_TZ)
    return {
        "type": "snapshot",
        "sent_at": local.isoformat(timespec="seconds"),
        "date": local.strftime("%m/%d"),
        "weekday": local.strftime("%a").upper(),
        "time": local.strftime("%H:%M"),
    }


def _label_for_window(window_minutes: int) -> str:
    if window_minutes == 300:
        return "5H LIMIT"
    if window_minutes == 10080:
        return "WEEK LIMIT"
    hours = max(1, round(window_minutes / 60))
    return f"{hours}H LIMIT"


def _reset_label(reset_at: int, now: datetime) -> str:
    reset_dt = datetime.fromtimestamp(reset_at, tz=LOCAL_TZ)
    local_now = now.astimezone(LOCAL_TZ)
    if reset_dt.date() == local_now.date():
        return reset_dt.strftime("%H:%M")
    return reset_dt.strftime("%m/%d")


def _window_snapshot(window: UsageWindow, now: datetime) -> dict:
    return {
        "label": _label_for_window(window.window_minutes),
        "remaining_percent": window.remaining_percent,
        "used_percent": window.used_percent,
        "reset_label": _reset_label(window.reset_at, now),
        "reset_at": window.reset_at,
        "window_minutes": window.window_minutes,
    }


def build_snapshot(usage: UsageStatus, now: datetime | None = None) -> dict:
    current = now or _now()
    snapshot = _base_snapshot(current)
    snapshot.update(
        {
            "plan_type": usage.plan_type,
            "primary": _window_snapshot(usage.primary, current),
            "secondary": _window_snapshot(usage.secondary, current),
            "status": "live",
        }
    )
    return snapshot


def build_error_snapshot(message: str, now: datetime | None = None) -> dict:
    snapshot = _base_snapshot(now or _now())
    snapshot.update({"status": "api_error", "message": message})
    return snapshot
```

- [ ] **Step 4: Run the snapshot tests and verify they pass**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_snapshot -v
```

Expected: `OK`.

- [ ] **Step 5: Commit Task 3**

Run:

```bash
git add host/token_light/snapshot.py tests/test_snapshot.py
git commit -m "feat: build display snapshots"
```

## Task 4: CLI, Mock Mode, And Serial Writer

**Files:**
- Create: `host/token_light/serial_writer.py`
- Create: `host/token_light/cli.py`
- Test: `tests/test_serial_writer.py`
- Test: `tests/test_cli.py`

- [ ] **Step 1: Write failing serial and CLI tests**

Create `tests/test_serial_writer.py`:

```python
import unittest

from token_light.serial_writer import encode_snapshot_line, write_snapshot


class FakeSerial:
    def __init__(self):
        self.data = b""
        self.flushed = False

    def write(self, chunk):
        self.data += chunk

    def flush(self):
        self.flushed = True


class SerialWriterTests(unittest.TestCase):
    def test_encode_snapshot_line_is_compact_json_with_newline(self):
        line = encode_snapshot_line({"type": "snapshot", "status": "live"})

        self.assertEqual(line, b'{"type":"snapshot","status":"live"}\n')

    def test_write_snapshot_flushes_serial_port(self):
        fake = FakeSerial()

        write_snapshot(fake, {"type": "snapshot"})

        self.assertEqual(fake.data, b'{"type":"snapshot"}\n')
        self.assertTrue(fake.flushed)


if __name__ == "__main__":
    unittest.main()
```

Create `tests/test_cli.py`:

```python
import json
import subprocess
import sys
import unittest
from pathlib import Path


FIXTURE = Path(__file__).parent / "fixtures" / "wham_usage.json"


class CliTests(unittest.TestCase):
    def test_mock_stdout_once_prints_snapshot_json(self):
        result = subprocess.run(
            [
                sys.executable,
                "-m",
                "token_light.cli",
                "--mock",
                str(FIXTURE),
                "--stdout",
                "--once",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        payload = json.loads(result.stdout)
        self.assertEqual(payload["type"], "snapshot")
        self.assertEqual(payload["status"], "live")
        self.assertEqual(payload["primary"]["remaining_percent"], 97)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the serial and CLI tests and verify they fail**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_serial_writer tests.test_cli -v
```

Expected: `ModuleNotFoundError` or CLI import failure for `token_light.serial_writer` and `token_light.cli`.

- [ ] **Step 3: Implement serial writer and CLI**

Create `host/token_light/serial_writer.py`:

```python
from __future__ import annotations

import json
from typing import Any

import serial
from serial.tools import list_ports


DEFAULT_PORT = "/dev/cu.usbmodem3101"
BAUD_RATE = 115200


def encode_snapshot_line(snapshot: dict[str, Any]) -> bytes:
    return (json.dumps(snapshot, separators=(",", ":"), ensure_ascii=True) + "\n").encode("utf-8")


def write_snapshot(port, snapshot: dict[str, Any]) -> None:
    port.write(encode_snapshot_line(snapshot))
    port.flush()


def open_serial_port(port_name: str = DEFAULT_PORT):
    return serial.Serial(port_name, BAUD_RATE, timeout=1)


def detected_serial_ports() -> list[str]:
    return [port.device for port in list_ports.comports() if port.device.startswith("/dev/cu.")]
```

Create `host/token_light/cli.py`:

```python
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

from token_light.auth import AuthError, DEFAULT_AUTH_FILE, read_access_token
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
    interval_seconds = args.interval
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
        time.sleep(interval_seconds)


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
```

- [ ] **Step 4: Run the serial and CLI tests and verify they pass**

Run:

```bash
PYTHONPATH=host python3 -m unittest tests.test_serial_writer tests.test_cli -v
```

Expected: `OK`.

- [ ] **Step 5: Verify all host tests pass**

Run:

```bash
PYTHONPATH=host python3 -m unittest discover -s tests -v
```

Expected: all tests show `ok` and the run ends with `OK`.

- [ ] **Step 6: Commit Task 4**

Run:

```bash
git add host/token_light/serial_writer.py host/token_light/cli.py tests/test_serial_writer.py tests/test_cli.py
git commit -m "feat: add token light sync cli"
```

## Task 5: Firmware Platform Scaffold And Waveshare Driver

**Files:**
- Create: `scripts/bootstrap_waveshare_driver.sh`
- Create: `firmware/platformio.ini`
- Create: `firmware/src/ST7305_U8g2.h`
- Create: `firmware/src/ST7305_U8g2.cpp`
- Create: `firmware/src/dashboard_state.h`
- Create: `firmware/src/dashboard_render.h`
- Create: `firmware/src/dashboard_render.cpp`
- Create: `firmware/src/main.cpp`

- [ ] **Step 1: Add the driver bootstrap script**

Create `scripts/bootstrap_waveshare_driver.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${TMPDIR:-/tmp}/token-light-waveshare"
REPO="https://github.com/waveshareteam/ESP32-S3-RLCD-4.2.git"
COMMIT="cb590c853f3d2bb58efe280c98b2f3cbf1e2876e"

rm -rf "$WORK"
git clone --depth 1 "$REPO" "$WORK"
git -C "$WORK" fetch --depth 1 origin "$COMMIT"
git -C "$WORK" checkout "$COMMIT"

mkdir -p "$ROOT/firmware/src"
cp "$WORK/02_Example/Arduino/10_U8G2_Test/ST7305_U8g2.h" "$ROOT/firmware/src/ST7305_U8g2.h"
cp "$WORK/02_Example/Arduino/10_U8G2_Test/ST7305_U8g2.cpp" "$ROOT/firmware/src/ST7305_U8g2.cpp"

echo "Copied Waveshare ST7305_U8g2 driver at $COMMIT"
```

Run:

```bash
chmod +x scripts/bootstrap_waveshare_driver.sh
./scripts/bootstrap_waveshare_driver.sh
```

Expected: `firmware/src/ST7305_U8g2.h` and `firmware/src/ST7305_U8g2.cpp` exist.

- [ ] **Step 2: Add PlatformIO configuration**

Create `firmware/platformio.ini`:

```ini
[env:esp32-s3-rlcd-4-2]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.psram_type = opi
board_upload.flash_size = 16MB
build_flags =
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DARDUINO_USB_MODE=1
  -DBOARD_HAS_PSRAM
lib_deps =
  olikraus/U8g2@^2.36.2
  bblanchon/ArduinoJson@^7.4.2
```

- [ ] **Step 3: Add firmware state and renderer headers**

Create `firmware/src/dashboard_state.h`:

```cpp
#pragma once

#include <Arduino.h>

struct LimitWindow {
  String label = "";
  int remainingPercent = 0;
  int usedPercent = 0;
  String resetLabel = "";
};

struct DisplaySnapshot {
  String date = "--/--";
  String weekday = "---";
  String time = "--:--";
  String status = "boot";
  String message = "";
  LimitWindow primary;
  LimitWindow secondary;
  unsigned long receivedAtMs = 0;
};
```

Create `firmware/src/dashboard_render.h`:

```cpp
#pragma once

#include <U8g2lib.h>
#include "dashboard_state.h"

void renderDashboard(U8G2 &u8g2, const DisplaySnapshot &snapshot, unsigned long nowMs);
```

- [ ] **Step 4: Add the first renderer implementation and main loop**

Create `firmware/src/dashboard_render.cpp`:

```cpp
#include "dashboard_render.h"

static const int LCD_WIDTH = 400;
static const int LCD_HEIGHT = 300;

static void drawProgress(U8G2 &u8g2, int x, int y, int w, int percent) {
  int clamped = constrain(percent, 0, 100);
  int fill = (w - 4) * clamped / 100;
  u8g2.drawFrame(x, y, w, 10);
  u8g2.drawBox(x + 2, y + 2, fill, 6);
}

static void drawMetricCard(U8G2 &u8g2, int x, int y, const LimitWindow &window) {
  u8g2.drawRFrame(x, y, 168, 88, 6);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(x + 14, y + 24, window.label.c_str());
  u8g2.setFont(u8g2_font_logisoso26_tn);
  char percent[8];
  snprintf(percent, sizeof(percent), "%d%%", window.remainingPercent);
  u8g2.drawStr(x + 14, y + 58, percent);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(x + 100, y + 56, "LEFT");
  drawProgress(u8g2, x + 14, y + 65, 140, window.remainingPercent);
  u8g2.setFont(u8g2_font_6x13B_tf);
  String reset = "RESET " + window.resetLabel;
  u8g2.drawStr(x + 14, y + 84, reset.c_str());
}

static void drawPixelPet(U8G2 &u8g2, int x, int y) {
  u8g2.drawFrame(x + 4, y + 4, 22, 18);
  u8g2.drawBox(x + 9, y + 10, 3, 3);
  u8g2.drawBox(x + 19, y + 10, 3, 3);
  u8g2.drawHLine(x + 11, y + 17, 9);
  u8g2.drawPixel(x + 2, y + 8);
  u8g2.drawPixel(x + 28, y + 8);
}

void renderDashboard(U8G2 &u8g2, const DisplaySnapshot &snapshot, unsigned long nowMs) {
  bool stale = snapshot.receivedAtMs > 0 && (nowMs - snapshot.receivedAtMs) > 300000UL;
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_9x18B_tf);
  String topLeft = snapshot.date + " " + snapshot.weekday;
  u8g2.drawStr(16, 28, topLeft.c_str());
  u8g2.drawStr(310, 28, stale ? "STALE" : "LIVE");

  u8g2.drawFrame(14, 44, 372, 88);
  u8g2.drawFrame(26, 56, 348, 64);
  u8g2.setFont(u8g2_font_logisoso78_tn);
  int clockWidth = u8g2.getStrWidth(snapshot.time.c_str());
  u8g2.drawStr((LCD_WIDTH - clockWidth) / 2, 116, snapshot.time.c_str());

  if (snapshot.status == "live") {
    drawMetricCard(u8g2, 20, 150, snapshot.primary);
    drawMetricCard(u8g2, 212, 150, snapshot.secondary);
  } else {
    u8g2.drawFrame(20, 150, 360, 88);
    u8g2.setFont(u8g2_font_9x18B_tf);
    u8g2.drawStr(40, 186, snapshot.status.c_str());
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(40, 212, snapshot.message.c_str());
  }

  u8g2.drawFrame(20, 255, 160, 28);
  u8g2.drawFrame(196, 255, 184, 28);
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(34, 276, stale ? "SYNC STALE" : "SYNC OK");
  drawPixelPet(u8g2, 330, 256);

  u8g2.sendBuffer();
}
```

Create `firmware/src/main.cpp`:

```cpp
#include <Arduino.h>
#include "ST7305_U8g2.h"
#include "dashboard_render.h"

#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41

static ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
static U8G2 *u8g2 = nullptr;
static DisplaySnapshot snapshot;

void setup() {
  Serial.begin(115200);
  delay(300);
  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();

  snapshot.date = "05/30";
  snapshot.weekday = "SAT";
  snapshot.time = "15:50";
  snapshot.status = "live";
  snapshot.primary = {"5H LIMIT", 97, 3, "20:36"};
  snapshot.secondary = {"WEEK LIMIT", 96, 4, "05/31"};
  snapshot.receivedAtMs = millis();
}

void loop() {
  renderDashboard(*u8g2, snapshot, millis());
  delay(1000);
}
```

- [ ] **Step 5: Install dependencies and build firmware**

Run:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -U pip
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/pio run -d firmware
```

Expected: PlatformIO build ends with `SUCCESS`.

- [ ] **Step 6: Commit Task 5**

Run:

```bash
git add scripts/bootstrap_waveshare_driver.sh firmware/platformio.ini firmware/src/ST7305_U8g2.h firmware/src/ST7305_U8g2.cpp firmware/src/dashboard_state.h firmware/src/dashboard_render.h firmware/src/dashboard_render.cpp firmware/src/main.cpp
git commit -m "feat: add esp32 dashboard scaffold"
```

## Task 6: Firmware Serial JSON Parser And Stale State

**Files:**
- Modify: `firmware/src/main.cpp`
- Modify: `firmware/src/dashboard_render.cpp`

- [ ] **Step 1: Add JSON parsing to `main.cpp`**

Replace the static-only `main.cpp` loop with:

```cpp
#include <Arduino.h>
#include <ArduinoJson.h>
#include "ST7305_U8g2.h"
#include "dashboard_render.h"

#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41

static ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
static U8G2 *u8g2 = nullptr;
static DisplaySnapshot snapshot;
static String lineBuffer;
static unsigned long lastRenderMs = 0;

static void setBootSnapshot() {
  snapshot.date = "--/--";
  snapshot.weekday = "---";
  snapshot.time = "--:--";
  snapshot.status = "boot";
  snapshot.message = "WAITING FOR HOST";
  snapshot.primary = {"5H LIMIT", 0, 0, "--:--"};
  snapshot.secondary = {"WEEK LIMIT", 0, 0, "--/--"};
  snapshot.receivedAtMs = 0;
}

static LimitWindow readWindow(JsonObject obj) {
  LimitWindow window;
  window.label = obj["label"] | "";
  window.remainingPercent = obj["remaining_percent"] | 0;
  window.usedPercent = obj["used_percent"] | 0;
  window.resetLabel = obj["reset_label"] | "";
  return window;
}

static void applySnapshot(JsonDocument &doc) {
  const char *type = doc["type"] | "";
  if (strcmp(type, "snapshot") != 0) {
    return;
  }
  snapshot.date = doc["date"] | snapshot.date;
  snapshot.weekday = doc["weekday"] | snapshot.weekday;
  snapshot.time = doc["time"] | snapshot.time;
  snapshot.status = doc["status"] | "api_error";
  snapshot.message = doc["message"] | "";
  if (snapshot.status == "live") {
    snapshot.primary = readWindow(doc["primary"].as<JsonObject>());
    snapshot.secondary = readWindow(doc["secondary"].as<JsonObject>());
  }
  snapshot.receivedAtMs = millis();
}

static void processSerialLine(const String &line) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, line);
  if (error) {
    return;
  }
  applySnapshot(doc);
}

static void readSerialInput() {
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\n') {
      processSerialLine(lineBuffer);
      lineBuffer = "";
    } else if (ch != '\r' && lineBuffer.length() < 1200) {
      lineBuffer += ch;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();
  setBootSnapshot();
}

void loop() {
  readSerialInput();
  unsigned long now = millis();
  if (now - lastRenderMs >= 1000UL) {
    renderDashboard(*u8g2, snapshot, now);
    lastRenderMs = now;
  }
  delay(5);
}
```

- [ ] **Step 2: Build firmware with serial parsing**

Run:

```bash
.venv/bin/pio run -d firmware
```

Expected: PlatformIO build ends with `SUCCESS`.

- [ ] **Step 3: Commit Task 6**

Run:

```bash
git add firmware/src/main.cpp firmware/src/dashboard_render.cpp
git commit -m "feat: parse dashboard snapshots on esp32"
```

## Task 7: Flash Board And Verify End To End

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add usage instructions to README**

Append this section to `README.md`:

````markdown
## Token Light

Token Light displays Codex usage-limit remaining percentages on a Waveshare ESP32-S3-RLCD-4.2 over USB serial.

### Setup

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -U pip
.venv/bin/python -m pip install -r requirements-dev.txt
```

### Build and flash

```bash
./scripts/bootstrap_waveshare_driver.sh
.venv/bin/pio run -d firmware
.venv/bin/pio run -d firmware -t upload --upload-port /dev/cu.usbmodem3101
```

### Test with mock data

```bash
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --stdout --once
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --port /dev/cu.usbmodem3101 --once
```

### Run live

```bash
.venv/bin/python -m token_light.cli --port /dev/cu.usbmodem3101
```

The host reads the existing Codex Desktop auth token from `~/.codex/auth.json` and sends only usage snapshots to the ESP32. Credentials stay on the Mac.
````

- [ ] **Step 2: Run complete host verification**

Run:

```bash
.venv/bin/python -m unittest discover -s tests -v
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --stdout --once
```

Expected: all Python tests pass, and the mock CLI prints one JSON object with `"status":"live"` and `"primary":{"label":"5H LIMIT"`.

- [ ] **Step 3: Flash the connected ESP32-S3-RLCD-4.2**

Run:

```bash
.venv/bin/pio run -d firmware -t upload --upload-port /dev/cu.usbmodem3101
```

Expected: upload ends with `Hard resetting via RTS pin` or PlatformIO `SUCCESS`.

- [ ] **Step 4: Send one mock snapshot to the board**

Run:

```bash
.venv/bin/python -m token_light.cli --mock tests/fixtures/wham_usage.json --port /dev/cu.usbmodem3101 --once
```

Expected on device: the screen shows `05/30 SAT`, a large `HH:MM` clock, `5H LIMIT 97% LEFT`, `WEEK LIMIT 96% LEFT`, reset labels `20:36` and `05/31`, and the lower sync strip shows `SYNC OK`.

- [ ] **Step 5: Run live Codex usage sync**

Run:

```bash
.venv/bin/python -m token_light.cli --port /dev/cu.usbmodem3101
```

Expected on device: the percentages and reset labels match the Codex app usage menu within one minute. The process stays running and sends a fresh snapshot every 60 seconds.

- [ ] **Step 6: Commit Task 7**

Run:

```bash
git add README.md
git commit -m "docs: add token light usage instructions"
```

## Self-Review

- Spec coverage:
  - Codex usage-limit data source is covered by Tasks 2, 3, 4, and 7.
  - No monthly budget or OpenAI billing data is used; host code reads the local Codex token and calls `/wham/usage`.
  - USB serial snapshots are covered by Tasks 4, 6, and 7.
  - Waveshare ESP32-S3-RLCD-4.2 display setup is covered by Task 5 using the official ST7305 U8g2 driver.
  - The black-and-white reference-style UI, large clock, two limit cards, bottom sync strip, and small pixel pet are covered by Task 5.
  - Stale data and API error states are covered by Tasks 3, 5, and 6.
- Placeholder scan:
  - No task uses vague implementation language; every code step includes concrete files, snippets, commands, and expected outputs.
  - The Waveshare driver source is pinned to commit `cb590c853f3d2bb58efe280c98b2f3cbf1e2876e`.
- Type consistency:
  - Host parser returns `UsageStatus` and `UsageWindow`; snapshot builder consumes those same dataclasses.
  - Host snapshot keys match firmware JSON parsing keys: `date`, `weekday`, `time`, `status`, `message`, `primary`, `secondary`, `label`, `remaining_percent`, `used_percent`, and `reset_label`.
  - Firmware render code uses `DisplaySnapshot` and `LimitWindow` fields defined in `dashboard_state.h`.
