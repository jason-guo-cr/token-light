import unittest

from token_light.companion import build_companion_payload, select_pet_pose


class CompanionTests(unittest.TestCase):
    def test_activity_states_map_to_expected_pet_poses(self):
        expected = {
            "idle": "sleep",
            "thinking": "working",
            "reading": "working",
            "editing": "coding",
            "testing": "testing",
            "working": "working",
            "waiting": "waiting",
            "done": "celebrate",
            "error": "alert",
        }

        for state, pose in expected.items():
            with self.subTest(state=state):
                self.assertEqual(select_pet_pose(state, remaining_percent=80), pose)

    def test_low_quota_is_tired_but_does_not_hide_terminal_states(self):
        self.assertEqual(select_pet_pose("editing", remaining_percent=10), "tired")
        self.assertEqual(select_pet_pose("done", remaining_percent=5), "celebrate")
        self.assertEqual(select_pet_pose("error", remaining_percent=5), "alert")

    def test_unknown_activity_falls_back_to_sleep(self):
        self.assertEqual(select_pet_pose("future-state", remaining_percent=80), "sleep")

    def test_payload_has_fixed_animation_contract(self):
        activity = {
            "state": "testing",
            "label": "TESTING",
            "detail": "TEST RUN",
            "elapsed_seconds": 92,
            "completion_seq": 7,
        }

        result = build_companion_payload(activity, remaining_percent=80)

        self.assertEqual(
            result,
            {
                "activity": activity,
                "pet": {"pose": "testing", "frame_count": 2, "frame_period_ms": 1000},
            },
        )

    def test_completion_celebration_expires_after_thirty_seconds(self):
        activity = {
            "state": "done",
            "label": "DONE",
            "detail": "TASK COMPLETE",
            "elapsed_seconds": 92,
            "completion_seq": 7,
            "_completion_age_seconds": 31,
        }

        result = build_companion_payload(activity, remaining_percent=80)

        self.assertEqual(result["pet"]["pose"], "sleep")
        self.assertNotIn("_completion_age_seconds", result["activity"])

    def test_payload_drops_unapproved_private_fields_and_text(self):
        activity = {
            "state": "testing",
            "label": "PRIVATE COMMAND",
            "detail": "/secret/project",
            "elapsed_seconds": 92,
            "completion_seq": 7,
            "prompt": "private prompt",
        }

        result = build_companion_payload(activity, remaining_percent=80)

        self.assertEqual(result["activity"]["label"], "TESTING")
        self.assertEqual(result["activity"]["detail"], "TEST RUN")
        self.assertNotIn("prompt", result["activity"])


if __name__ == "__main__":
    unittest.main()
