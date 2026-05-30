import json
import tempfile
import unittest
from pathlib import Path

from token_light.auth import AuthError, read_access_token


class AuthTests(unittest.TestCase):
    def test_reads_access_token_from_codex_auth_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            auth_path = Path(tmp) / "auth.json"
            auth_path.write_text(
                json.dumps({"tokens": {"access_token": "secret-token"}}),
                encoding="utf-8",
            )

            self.assertEqual(read_access_token(auth_path), "secret-token")

    def test_missing_token_raises_clear_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            auth_path = Path(tmp) / "auth.json"
            auth_path.write_text(json.dumps({"tokens": {}}), encoding="utf-8")

            with self.assertRaisesRegex(AuthError, "access token"):
                read_access_token(auth_path)


if __name__ == "__main__":
    unittest.main()
