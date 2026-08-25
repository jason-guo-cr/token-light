import unittest
from datetime import datetime
from zoneinfo import ZoneInfo

from token_light.codex_usage import UsageStatus, UsageWindow
from token_light.snapshot import build_error_snapshot, build_snapshot


class CompanionSnapshotTests(unittest.TestCase):
    def setUp(self):
        self.now = datetime(2026, 8, 25, 19, 0, tzinfo=ZoneInfo("Asia/Shanghai"))
        self.usage = UsageStatus(
            plan_type="pro",
            windows=(UsageWindow(used_percent=20, remaining_percent=80, window_minutes=10080,
                                 reset_at=1788170400),),
        )
        self.companion = {
            "activity": {"state": "testing", "label": "TESTING", "detail": "TEST RUN",
                         "elapsed_seconds": 92, "completion_seq": 7},
            "pet": {"pose": "testing", "frame_count": 2, "frame_period_ms": 1000},
        }
        self.forecast = {
            "pace": "normal",
            "pace_label": "NORMAL",
            "quota_points_per_hour": 0.8,
            "projected_used_percent": 32,
            "forecast_label": "EST 32%",
        }

    def test_live_snapshot_includes_companion_forecast_and_audio_contract(self):
        result = build_snapshot(
            self.usage,
            now=self.now,
            companion=self.companion,
            forecast=self.forecast,
            audio={"enabled": True, "quiet": False},
        )

        self.assertEqual(result["companion"], self.companion)
        self.assertEqual(result["forecast"], self.forecast)
        self.assertEqual(result["audio"], {"enabled": True, "quiet": False})

    def test_error_snapshot_keeps_last_good_optional_data(self):
        result = build_error_snapshot(
            "SYNC UNAVAILABLE",
            now=self.now,
            companion=self.companion,
            forecast=self.forecast,
        )

        self.assertEqual(result["status"], "api_error")
        self.assertEqual(result["companion"], self.companion)
        self.assertEqual(result["forecast"], self.forecast)

    def test_all_new_objects_remain_optional_for_old_hosts(self):
        result = build_snapshot(self.usage, now=self.now)

        self.assertNotIn("companion", result)
        self.assertNotIn("forecast", result)
        self.assertNotIn("audio", result)


if __name__ == "__main__":
    unittest.main()
