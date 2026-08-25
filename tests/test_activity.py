import json
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from token_light.activity import read_codex_activity


TZ = ZoneInfo("Asia/Shanghai")


class ActivityTests(unittest.TestCase):
    def test_missing_session_directories_are_idle(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = read_codex_activity(Path(tmp), datetime(2026, 8, 25, 19, 0, tzinfo=TZ))

        self.assertEqual(
            result,
            {"state": "idle", "label": "IDLE", "detail": "NO ACTIVE TASK", "elapsed_seconds": 0,
             "completion_seq": 0},
        )

    def test_task_start_is_thinking_and_reports_elapsed_time(self):
        result = self._read([self._event("2026-08-25T10:58:30Z", "task_started")])

        self.assertEqual(result["state"], "thinking")
        self.assertEqual(result["label"], "THINKING")
        self.assertEqual(result["detail"], "PLANNING")
        self.assertEqual(result["elapsed_seconds"], 90)

    def test_patch_web_test_and_other_tools_have_sanitized_states(self):
        cases = (
            ("apply_patch", "private patch /secret/file", "editing", "CODE CHANGE"),
            ("web__run", "private search query", "reading", "RESEARCH"),
            ("exec", ".venv/bin/python -m unittest discover -s /secret/tests", "testing", "TEST RUN"),
            ("exec", "git status --short /secret/project", "working", "TOOL RUN"),
        )

        for name, tool_input, state, detail in cases:
            with self.subTest(name=name, state=state):
                result = self._read(
                    [
                        self._event("2026-08-25T10:58:00Z", "task_started"),
                        self._tool("2026-08-25T10:59:50Z", name, tool_input),
                    ]
                )
                encoded = json.dumps(result).lower()
                self.assertEqual(result["state"], state)
                self.assertEqual(result["detail"], detail)
                self.assertNotIn("secret", encoded)
                self.assertNotIn("unittest", encoded)

    def test_completed_task_is_done_for_two_minutes_then_idle(self):
        events = [
            self._event("2026-08-25T10:55:00Z", "task_started"),
            self._event("2026-08-25T10:59:30Z", "task_complete"),
        ]

        recent = self._read(events, datetime(2026, 8, 25, 19, 1, tzinfo=TZ))
        expired = self._read(events, datetime(2026, 8, 25, 19, 1, 31, tzinfo=TZ))

        self.assertEqual(recent["state"], "done")
        self.assertEqual(recent["detail"], "TASK COMPLETE")
        self.assertEqual(recent["elapsed_seconds"], 270)
        self.assertEqual(recent["completion_seq"], 1)
        self.assertEqual(expired["state"], "idle")

    def test_aborted_turn_is_stopped(self):
        result = self._read(
            [
                self._event("2026-08-25T10:58:00Z", "task_started"),
                self._event("2026-08-25T10:59:30Z", "turn_aborted"),
            ]
        )

        self.assertEqual(result["state"], "error")
        self.assertEqual(result["label"], "STOPPED")
        self.assertEqual(result["detail"], "TASK STOPPED")

    def test_open_task_without_events_for_five_minutes_is_waiting(self):
        result = self._read(
            [self._event("2026-08-25T10:50:00Z", "task_started")],
            datetime(2026, 8, 25, 19, 0, tzinfo=TZ),
        )

        self.assertEqual(result["state"], "waiting")
        self.assertEqual(result["detail"], "NO RECENT EVENTS")

    def test_malformed_and_unknown_lines_are_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            path = codex_home / "sessions/rollout.jsonl"
            path.parent.mkdir(parents=True)
            path.write_text(
                "not-json\n"
                + json.dumps(self._event("2026-08-25T10:57:00Z", "future_event"))
                + "\n"
                + json.dumps(self._event("2026-08-25T10:59:50Z", "task_started"))
                + "\n",
                encoding="utf-8",
            )

            result = read_codex_activity(codex_home, datetime(2026, 8, 25, 19, 0, tzinfo=TZ))

        self.assertEqual(result["state"], "thinking")

    def test_newest_event_timestamp_wins_across_active_and_archived_logs(self):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            self._write(
                codex_home / "sessions/z-old-name.jsonl",
                [self._event("2026-08-25T10:55:00Z", "task_started"),
                 self._event("2026-08-25T10:55:30Z", "task_complete")],
            )
            self._write(
                codex_home / "archived_sessions/a-new-name.jsonl",
                [self._event("2026-08-25T10:59:50Z", "task_started")],
            )

            result = read_codex_activity(codex_home, datetime(2026, 8, 25, 19, 0, tzinfo=TZ))

        self.assertEqual(result["state"], "thinking")
        self.assertEqual(result["elapsed_seconds"], 10)

    def _read(self, events, now=None):
        with tempfile.TemporaryDirectory() as tmp:
            codex_home = Path(tmp)
            self._write(codex_home / "sessions/rollout.jsonl", events)
            return read_codex_activity(
                codex_home,
                now or datetime(2026, 8, 25, 19, 0, tzinfo=TZ),
            )

    @staticmethod
    def _write(path: Path, events):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(json.dumps(event) for event in events) + "\n", encoding="utf-8")

    @staticmethod
    def _event(timestamp, payload_type):
        return {"timestamp": timestamp, "type": "event_msg", "payload": {"type": payload_type}}

    @staticmethod
    def _tool(timestamp, name, tool_input):
        return {
            "timestamp": timestamp,
            "type": "response_item",
            "payload": {"type": "custom_tool_call", "name": name, "status": "completed", "input": tool_input},
        }


if __name__ == "__main__":
    unittest.main()
