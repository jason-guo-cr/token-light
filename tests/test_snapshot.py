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

        snapshot = build_snapshot(
            usage,
            now=now,
            battery={"percent": 61, "charging": False, "state": "discharging"},
            token_usage={"today_total": 3_500_000, "week_total": 5_500_000, "today_label": "3.5M", "week_label": "5.5M"},
            limit_updated_at=datetime(2026, 5, 30, 15, 42, tzinfo=ZoneInfo("Asia/Shanghai")),
            weather={"display": "BJ 24C", "temperature_c": 24},
        )

        self.assertEqual(snapshot["type"], "snapshot")
        self.assertEqual(snapshot["sent_at"], "2026-05-30T15:50:00+08:00")
        self.assertEqual(snapshot["date"], "05/30")
        self.assertEqual(snapshot["weekday"], "SAT")
        self.assertEqual(snapshot["time"], "15:50")
        self.assertEqual(snapshot["plan_type"], "prolite")
        self.assertEqual(snapshot["primary"]["label"], "5H LIMIT")
        self.assertEqual(snapshot["primary"]["remaining_percent"], 97)
        self.assertEqual(snapshot["primary"]["used_percent"], 3)
        self.assertEqual(snapshot["primary"]["reset_label"], "20:36")
        self.assertEqual(snapshot["primary"]["reset_at"], 1780144603)
        self.assertEqual(snapshot["primary"]["window_minutes"], 300)
        self.assertEqual(snapshot["secondary"]["label"], "WEEK LIMIT")
        self.assertEqual(snapshot["secondary"]["remaining_percent"], 96)
        self.assertEqual(snapshot["secondary"]["used_percent"], 4)
        self.assertEqual(snapshot["secondary"]["reset_label"], "05/31")
        self.assertEqual(snapshot["secondary"]["reset_at"], 1780217709)
        self.assertEqual(snapshot["secondary"]["window_minutes"], 10080)
        self.assertEqual(snapshot["battery"]["percent"], 61)
        self.assertFalse(snapshot["battery"]["charging"])
        self.assertEqual(snapshot["token_usage"]["today_label"], "3.5M")
        self.assertEqual(snapshot["token_usage"]["week_label"], "5.5M")
        self.assertEqual(snapshot["limit_updated_label"], "15:42")
        self.assertEqual(snapshot["weather"]["display"], "BJ 24C")
        self.assertEqual(snapshot["status"], "live")

    def test_naive_now_is_treated_as_shanghai_local_time(self):
        usage = UsageStatus(
            plan_type="prolite",
            primary=UsageWindow(used_percent=3, remaining_percent=97, window_minutes=300, reset_at=1780144603),
            secondary=UsageWindow(used_percent=4, remaining_percent=96, window_minutes=10080, reset_at=1780217709),
        )
        now = datetime(2026, 5, 30, 15, 50)

        snapshot = build_snapshot(usage, now=now)

        self.assertEqual(snapshot["sent_at"], "2026-05-30T15:50:00+08:00")
        self.assertEqual(snapshot["time"], "15:50")
        self.assertEqual(snapshot["weekday"], "SAT")

    def test_error_snapshot_keeps_time_and_message(self):
        now = datetime(2026, 5, 30, 15, 50, tzinfo=ZoneInfo("Asia/Shanghai"))

        snapshot = build_error_snapshot(
            "Codex usage request failed",
            now=now,
            battery={"percent": 61, "charging": False, "state": "discharging"},
            limit_updated_at=datetime(2026, 5, 30, 15, 42, tzinfo=ZoneInfo("Asia/Shanghai")),
            weather={"display": "BJ 24C", "temperature_c": 24},
        )

        self.assertEqual(snapshot["type"], "snapshot")
        self.assertEqual(snapshot["sent_at"], "2026-05-30T15:50:00+08:00")
        self.assertEqual(snapshot["date"], "05/30")
        self.assertEqual(snapshot["weekday"], "SAT")
        self.assertEqual(snapshot["status"], "api_error")
        self.assertEqual(snapshot["message"], "Codex usage request failed")
        self.assertEqual(snapshot["battery"]["percent"], 61)
        self.assertEqual(snapshot["time"], "15:50")
        self.assertEqual(snapshot["limit_updated_label"], "15:42")
        self.assertEqual(snapshot["weather"]["display"], "BJ 24C")


if __name__ == "__main__":
    unittest.main()
