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
