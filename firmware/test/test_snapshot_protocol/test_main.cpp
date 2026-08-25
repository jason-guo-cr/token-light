#include <ArduinoJson.h>
#include <unity.h>

#include "snapshot_protocol.h"

static SnapshotProtocolResult applyJson(const char *json, SnapshotProtocolState &state) {
  JsonDocument document;
  TEST_ASSERT_FALSE(deserializeJson(document, json));
  return applySnapshotProtocol(document, state);
}

static void test_old_live_snapshot_remains_compatible() {
  SnapshotProtocolState state;

  const SnapshotProtocolResult result = applyJson(
      R"({"type":"snapshot","status":"live","time":"19:08","primary":{"label":"CODEX WEEK","remaining_percent":76,"used_percent":24,"reset_label":"08/31"}})",
      state);

  TEST_ASSERT_TRUE(result.accepted);
  TEST_ASSERT_FALSE(result.hasCompletionSequence);
  TEST_ASSERT_TRUE(state.primary.valid);
  TEST_ASSERT_EQUAL_INT(76, state.primary.remainingPercent);
  TEST_ASSERT_EQUAL_INT(24, state.primary.usedPercent);
  TEST_ASSERT_EQUAL_STRING("19:08", state.time.c_str());
  TEST_ASSERT_EQUAL_STRING("sleep", state.petPose.c_str());
}

static void test_first_error_snapshot_keeps_quota_unavailable_and_message_visible() {
  SnapshotProtocolState state;

  applyJson(
      R"({"type":"snapshot","status":"api_error","message":"SYNC UNAVAILABLE","time":"19:08"})",
      state);

  TEST_ASSERT_FALSE(state.primary.valid);
  TEST_ASSERT_EQUAL_STRING("SYNC UNAVAILABLE", state.message.c_str());
  TEST_ASSERT_EQUAL_STRING("api_error", state.status.c_str());
}

static void test_wrong_numeric_types_preserve_last_valid_values() {
  SnapshotProtocolState state;
  applyJson(
      R"({"type":"snapshot","status":"live","primary":{"remaining_percent":76,"used_percent":24},"companion":{"activity":{"elapsed_seconds":92,"completion_seq":7}}})",
      state);

  const SnapshotProtocolResult result = applyJson(
      R"({"type":"snapshot","status":"live","primary":{"remaining_percent":"0","used_percent":false},"companion":{"activity":{"elapsed_seconds":"0","completion_seq":-1}}})",
      state);

  TEST_ASSERT_TRUE(state.primary.valid);
  TEST_ASSERT_EQUAL_INT(76, state.primary.remainingPercent);
  TEST_ASSERT_EQUAL_INT(24, state.primary.usedPercent);
  TEST_ASSERT_EQUAL_UINT32(92, state.activityElapsedSeconds);
  TEST_ASSERT_EQUAL_UINT32(7, state.completionSequence);
  TEST_ASSERT_FALSE(result.hasCompletionSequence);
}

static void test_new_protocol_fields_are_sanitized_and_parsed() {
  SnapshotProtocolState state;

  const SnapshotProtocolResult result = applyJson(
      R"({"type":"snapshot","status":"live","companion":{"activity":{"state":"testing","label":"PRIVATE","detail":"/secret/path","elapsed_seconds":92,"completion_seq":7},"pet":{"pose":"celebrate"}},"forecast":{"pace":"normal","projected_used_percent":32},"audio":{"enabled":true,"quiet":false}})",
      state);

  TEST_ASSERT_TRUE(result.hasCompletionSequence);
  TEST_ASSERT_EQUAL_STRING("testing", state.activityState.c_str());
  TEST_ASSERT_EQUAL_STRING("TESTING", state.activityLabel.c_str());
  TEST_ASSERT_EQUAL_STRING("TEST RUN", state.activityDetail.c_str());
  TEST_ASSERT_EQUAL_STRING("celebrate", state.petPose.c_str());
  TEST_ASSERT_EQUAL_STRING("NORMAL", state.paceLabel.c_str());
  TEST_ASSERT_EQUAL_STRING("EST 32%", state.forecastLabel.c_str());
  TEST_ASSERT_TRUE(state.audioEnabled);
  TEST_ASSERT_FALSE(state.audioQuiet);
}

static void test_non_snapshot_is_rejected_without_mutation() {
  SnapshotProtocolState state;

  const SnapshotProtocolResult result = applyJson(
      R"({"type":"event","status":"live","primary":{"remaining_percent":0}})", state);

  TEST_ASSERT_FALSE(result.accepted);
  TEST_ASSERT_EQUAL_STRING("boot", state.status.c_str());
  TEST_ASSERT_FALSE(state.primary.valid);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_old_live_snapshot_remains_compatible);
  RUN_TEST(test_first_error_snapshot_keeps_quota_unavailable_and_message_visible);
  RUN_TEST(test_wrong_numeric_types_preserve_last_valid_values);
  RUN_TEST(test_new_protocol_fields_are_sanitized_and_parsed);
  RUN_TEST(test_non_snapshot_is_rejected_without_mutation);
  return UNITY_END();
}
