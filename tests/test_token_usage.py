import json
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from token_light.token_usage import build_token_usage_snapshot, iter_token_count_events


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

    def test_malformed_token_count_shapes_and_values_are_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            log = codex_home / "sessions/rollout.jsonl"
            log.parent.mkdir(parents=True)
            valid = self._event("2026-05-30T08:00:00Z", 12)
            malformed = [
                {"timestamp": "2026-05-30T08:00:01Z", "payload": []},
                {"timestamp": "2026-05-30T08:00:02Z", "payload": {"type": "token_count", "info": []}},
                {"timestamp": "2026-05-30T08:00:03Z", "payload": {"type": "token_count", "info": {"last_token_usage": []}}},
                self._event("2026-05-30T08:00:04Z", float("nan")),
                self._event("2026-05-30T08:00:05Z", "corrupt"),
                self._event("2026-05-30T08:00:06Z", True),
                self._event("2026-05-30T08:00:07Z", -1),
                self._event("2026-05-30T08:00:08Z", 1e20),
            ]
            log.write_text(
                "\n".join(json.dumps(event) for event in [*malformed, valid]) + "\n",
                encoding="utf-8",
            )

            events = list(iter_token_count_events(codex_home))
            snapshot = build_token_usage_snapshot(
                codex_home,
                now=datetime(2026, 5, 30, 17, 0, tzinfo=ZoneInfo("Asia/Shanghai")),
            )

        self.assertEqual([event["total_tokens"] for event in events], [12])
        self.assertEqual(snapshot["today_total"], 12)

    def test_invalid_optional_token_value_discards_the_whole_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            log = codex_home / "sessions/rollout.jsonl"
            log.parent.mkdir(parents=True)
            event = self._event("2026-05-30T08:00:00Z", 12)
            event["payload"]["info"]["last_token_usage"]["output_tokens"] = "corrupt"
            log.write_text(json.dumps(event) + "\n", encoding="utf-8")

            self.assertEqual(list(iter_token_count_events(codex_home)), [])

    def test_oversized_integer_discards_only_the_bad_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            log = codex_home / "sessions/rollout.jsonl"
            log.parent.mkdir(parents=True)
            oversized = self._event("2026-05-30T08:00:00Z", 10**400)
            valid = self._event("2026-05-30T08:00:01Z", 12)
            log.write_text(
                "\n".join(json.dumps(event) for event in (oversized, valid)) + "\n",
                encoding="utf-8",
            )

            events = list(iter_token_count_events(codex_home))

        self.assertEqual([event["total_tokens"] for event in events], [12])

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
