import contextlib
import io
import unittest

from token_light.cli import main


class CliStubTests(unittest.TestCase):
    def test_cli_stub_returns_nonzero_and_explains_not_implemented(self):
        stderr = io.StringIO()

        with contextlib.redirect_stderr(stderr):
            status = main([])

        self.assertEqual(status, 2)
        self.assertIn("not implemented", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
