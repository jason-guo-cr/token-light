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
