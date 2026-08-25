import json
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from token_light.burn_rate import build_burn_rate_snapshot, load_quota_history


UTC = ZoneInfo("UTC")


class BurnRateTests(unittest.TestCase):
    def setUp(self):
        self.now = datetime(2026, 8, 25, 11, 0, tzinfo=UTC)
        self.window = {
            "limit_id": "codex",
            "reset_at": int(datetime(2026, 8, 25, 14, 0, tzinfo=UTC).timestamp()),
            "used_percent": 20,
        }

    def test_sixty_minute_window_excludes_left_boundary(self):
        events = [
            {"timestamp": "2026-08-25T10:00:00Z", "total_tokens": 900_000},
            {"timestamp": "2026-08-25T10:00:01Z", "total_tokens": 1_200_000},
            {"timestamp": "2026-08-25T10:45:00Z", "total_tokens": 600_000},
        ]

        result = build_burn_rate_snapshot(events, [], self.window, self.now)

        self.assertEqual(result["burn_60m"], 1_800_000)
        self.assertEqual(result["burn_label"], "1.8M/H")

    def test_no_recent_tokens_formats_as_zero_per_hour(self):
        result = build_burn_rate_snapshot([], [], self.window, self.now)

        self.assertEqual(result["burn_60m"], 0)
        self.assertEqual(result["burn_label"], "0/H")

    def test_linear_quota_slope_projects_usage_at_reset(self):
        samples = [
            self._sample("2026-08-25T09:00:00Z", 16),
            self._sample("2026-08-25T10:00:00Z", 18),
            self._sample("2026-08-25T11:00:00Z", 20),
        ]

        result = build_burn_rate_snapshot([], samples, self.window, self.now)

        self.assertAlmostEqual(result["quota_points_per_hour"], 2.0)
        self.assertEqual(result["projected_used_percent"], 26)
        self.assertEqual(result["forecast_label"], "EST 26%")
        self.assertEqual(result["pace"], "hot")
        self.assertEqual(result["pace_label"], "HOT")

    def test_samples_from_another_reset_window_are_not_mixed(self):
        samples = [
            self._sample("2026-08-25T09:00:00Z", 99, reset_at=self.window["reset_at"] - 86400),
            self._sample("2026-08-25T10:00:00Z", 19),
            self._sample("2026-08-25T11:00:00Z", 20),
        ]

        result = build_burn_rate_snapshot([], samples, self.window, self.now)

        self.assertAlmostEqual(result["quota_points_per_hour"], 1.0)
        self.assertEqual(result["projected_used_percent"], 23)

    def test_insufficient_history_has_explicit_unknown_forecast(self):
        result = build_burn_rate_snapshot(
            [],
            [self._sample("2026-08-25T10:45:00Z", 20)],
            self.window,
            self.now,
        )

        self.assertIsNone(result["quota_points_per_hour"])
        self.assertIsNone(result["projected_used_percent"])
        self.assertEqual(result["forecast_label"], "EST --")
        self.assertEqual(result["pace"], "unknown")

    def test_negative_slope_is_zero_and_projection_is_clamped(self):
        falling = [self._sample("2026-08-25T10:00:00Z", 22), self._sample("2026-08-25T11:00:00Z", 20)]
        hot_window = dict(self.window, used_percent=98)
        rising = [self._sample("2026-08-25T10:00:00Z", 90), self._sample("2026-08-25T11:00:00Z", 98)]

        falling_result = build_burn_rate_snapshot([], falling, self.window, self.now)
        rising_result = build_burn_rate_snapshot([], rising, hot_window, self.now)

        self.assertEqual(falling_result["quota_points_per_hour"], 0)
        self.assertEqual(falling_result["projected_used_percent"], 20)
        self.assertEqual(rising_result["projected_used_percent"], 100)

    def test_history_loader_ignores_bad_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "quota-history.jsonl"
            valid = self._sample("2026-08-25T10:00:00Z", 18)
            path.write_text("bad json\n" + json.dumps(valid) + "\n", encoding="utf-8")

            result = load_quota_history(path)

        self.assertEqual(result, [valid])

    def _sample(self, observed_at, used_percent, reset_at=None):
        return {
            "observed_at": observed_at,
            "limit_id": "codex",
            "reset_at": self.window["reset_at"] if reset_at is None else reset_at,
            "used_percent": used_percent,
        }


if __name__ == "__main__":
    unittest.main()
