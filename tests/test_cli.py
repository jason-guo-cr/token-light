import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

from token_light.cli import UsagePoller, _build_snapshot, _load_mock, build_parser
from token_light.codex_usage import UsageFetchError

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
        self.assertEqual(payload["primary"]["remaining_percent"], 96)
        self.assertNotIn("secondary", payload)

    def test_defaults_refresh_screen_every_minute_but_usage_every_ten_minutes(self):
        args = build_parser().parse_args([])

        self.assertEqual(args.interval, 60)
        self.assertEqual(args.usage_interval, 600)

    def test_usage_poller_reuses_usage_between_api_intervals(self):
        args = build_parser().parse_args(
            [
                "--mock",
                str(FIXTURE),
                "--no-battery",
                "--no-token-usage",
            ]
        )
        poller = UsagePoller()

        with patch("token_light.cli._load_mock", wraps=_load_mock) as load_mock:
            _build_snapshot(args, usage_poller=poller, now_monotonic=0)
            _build_snapshot(args, usage_poller=poller, now_monotonic=60)
            _build_snapshot(args, usage_poller=poller, now_monotonic=600)

        self.assertEqual(load_mock.call_count, 2)

    def test_usage_poller_waits_between_failed_api_attempts(self):
        args = build_parser().parse_args(
            [
                "--no-battery",
                "--no-token-usage",
            ]
        )
        poller = UsagePoller()

        with patch("token_light.cli._read_usage", side_effect=UsageFetchError("rate limited")) as read_usage:
            first = _build_snapshot(args, usage_poller=poller, now_monotonic=0)
            second = _build_snapshot(args, usage_poller=poller, now_monotonic=60)
            third = _build_snapshot(args, usage_poller=poller, now_monotonic=600)

        self.assertEqual(first["status"], "api_error")
        self.assertEqual(second["status"], "api_error")
        self.assertEqual(third["status"], "api_error")
        self.assertEqual(read_usage.call_count, 2)

    def test_usage_poller_keeps_last_good_data_as_cached_after_failure(self):
        args = build_parser().parse_args(["--no-battery", "--no-token-usage", "--no-weather"])
        poller = UsagePoller()

        with patch(
            "token_light.cli._read_usage",
            side_effect=[_load_mock(FIXTURE), UsageFetchError("offline", "SYNC UNAVAILABLE")],
        ):
            live = _build_snapshot(args, usage_poller=poller, now_monotonic=0)
            cached = _build_snapshot(args, usage_poller=poller, now_monotonic=600)

        self.assertEqual(live["status"], "live")
        self.assertEqual(cached["status"], "cached")
        self.assertEqual(cached["warning"], "SYNC UNAVAILABLE")
        self.assertEqual(cached["primary"]["remaining_percent"], 96)


if __name__ == "__main__":
    unittest.main()
