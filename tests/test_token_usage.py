import json
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from token_light.token_usage import build_token_usage_snapshot


class TokenUsageTests(unittest.TestCase):
    def test_build_token_usage_snapshot_sums_today_and_week(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            session_dir = codex_home / "sessions"
            session_dir.mkdir()
            log = session_dir / "2026-05-30-session.jsonl"
            events = [
                self._event("2026-05-26T03:00:00Z", 2_000_000),
                self._event("2026-05-30T02:00:00Z", 1_500_000),
                self._event("2026-05-30T08:00:00Z", 2_000_000),
            ]
            log.write_text("\n".join(json.dumps(event) for event in events), encoding="utf-8")

            snapshot = build_token_usage_snapshot(
                codex_home,
                now=datetime(2026, 5, 30, 17, 0, tzinfo=ZoneInfo("Asia/Shanghai")),
            )

            self.assertEqual(snapshot["today_total"], 3_500_000)
            self.assertEqual(snapshot["week_total"], 5_500_000)
            self.assertEqual(snapshot["today_label"], "3.5M")
            self.assertEqual(snapshot["week_label"], "5.5M")

    @staticmethod
    def _event(timestamp, total):
        return {
            "timestamp": timestamp,
            "payload": {
                "type": "token_count",
                "info": {
                    "last_token_usage": {
                        "total_tokens": total,
                        "input_tokens": total,
                        "cached_input_tokens": 0,
                        "output_tokens": 0,
                    }
                },
            },
        }


if __name__ == "__main__":
    unittest.main()
