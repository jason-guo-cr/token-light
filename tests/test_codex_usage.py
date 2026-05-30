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
        self.assertEqual(usage.secondary.reset_at, 1780217709)

    def test_remaining_percent_is_clamped(self):
        payload = {
            "plan_type": "prolite",
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

        self.assertEqual(usage.plan_type, "prolite")
        self.assertEqual(usage.primary.remaining_percent, 100)
        self.assertEqual(usage.secondary.remaining_percent, 0)

    def test_missing_rate_limit_raises_parse_error(self):
        with self.assertRaisesRegex(UsageParseError, "rate_limit"):
            parse_usage({})


if __name__ == "__main__":
    unittest.main()
