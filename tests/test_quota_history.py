import json
import tempfile
import unittest
from datetime import datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

from token_light.quota_history import record_quota_sample


UTC = ZoneInfo("UTC")


class QuotaHistoryTests(unittest.TestCase):
    def test_record_keeps_only_protocol_fields_and_trims_old_samples(self):
        now = datetime(2026, 8, 25, 11, 0, tzinfo=UTC)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "quota-history.jsonl"
            old = {
                "observed_at": (now - timedelta(days=15)).isoformat(),
                "limit_id": "codex",
                "reset_at": 1787650000,
                "used_percent": 10,
            }
            path.write_text(json.dumps(old) + "\n", encoding="utf-8")

            record_quota_sample(
                path,
                {"limit_id": "codex", "reset_at": 1787656800, "used_percent": 20, "token": "secret"},
                now,
            )
            lines = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]

        self.assertEqual(len(lines), 1)
        self.assertEqual(
            set(lines[0]), {"observed_at", "limit_id", "reset_at", "used_percent"}
        )
        self.assertNotIn("secret", json.dumps(lines))
