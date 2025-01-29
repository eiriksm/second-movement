#include "unity/src/unity.h"
#include "src/unit_counter_face.h"
#include "movement.h"
#include <stdlib.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_loop_simple_unit(void) {
  void *context_ptr = malloc(sizeof(unit_counter_state_t));
  TEST_ASSERT_EQUAL(true, unit_counter_face_loop((movement_event_t){.event_type = EVENT_ALARM_LONG_PRESS}, context_ptr));
}

void test_delete_unit(void) {
  void *context_ptr = malloc(sizeof(unit_counter_state_t));
  // Cast to state object?
  unit_counter_state_t *state = (unit_counter_state_t *)context_ptr;
  state->unit_count = 2;
  state->screen_delta = 1;
  state->units[0].volume = 300;
  state->units[0].percentage = 50;
  state->units[1].volume = 330;
  state->units[1].percentage = 60;
  state->edit_offset = 1;
  unit_counter_face_loop((movement_event_t){.event_type = EVENT_ALARM_LONG_PRESS}, state);
  TEST_ASSERT_EQUAL(1, state->unit_count);
  TEST_ASSERT_EQUAL(330, state->units[0].volume);
  TEST_ASSERT_EQUAL(60, state->units[0].percentage);
  TEST_ASSERT_NOT_EQUAL(300, state->units[1].volume);
  TEST_ASSERT_NOT_EQUAL(50, state->units[1].percentage);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_simple_unit);
  RUN_TEST(test_delete_unit);
  return UNITY_END();
}
