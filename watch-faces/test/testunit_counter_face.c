#include "unity/src/unity.h"
#include "src/unit_counter_face.h"
#include "movement.h"
#include <stdlib.h>
#include <time.h>

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

void test_time_runs_out_add_unit(void) {
  void *context_ptr = malloc(sizeof(unit_counter_state_t));
  // Cast to state object
  unit_counter_state_t *state = (unit_counter_state_t *)context_ptr;
  // Set the start time to 10 minutes ago, UTC time.
  time_t current_time;
  current_time = time(NULL);
  struct tm *time_info;
  time_info = gmtime(&current_time);
  time_t current_timestamp = mktime(time_info);
  state->start_time = current_timestamp - 600;
  // Let's add 2 units.
  state->unit_count = 2;
  state->units[0].volume = 300;
  state->units[0].percentage = 50;
  state->units[1].volume = 330;
  state->units[1].percentage = 60;
  // Guess I also need a weight.
  state->weight = 80;
  unit_counter_face_activate(state);
  unit_counter_face_loop((movement_event_t){.event_type = EVENT_TICK}, state);
  // Check the BAC.
  float bac = unit_counter_calculate_bac(state);
  TEST_ASSERT_EQUAL_FLOAT(0.479728, bac);
  // Now let's make the time run out, and then check the BAC again. Let's say 10 hours have passed.
  state->start_time = current_timestamp - 36000;
  unit_counter_face_activate(state);
  unit_counter_face_loop((movement_event_t){.event_type = EVENT_TICK}, state);
  bac = unit_counter_calculate_bac(state);
  TEST_ASSERT_EQUAL_FLOAT(0.0, bac);
  // Now pretend we are adding a unit.
  unit_counter_face_activate(state);
  unit_counter_face_loop((movement_event_t){.event_type = EVENT_ALARM_BUTTON_UP}, state);
  TEST_ASSERT_EQUAL(1, state->unit_count);
  bac = unit_counter_calculate_bac(state);
  TEST_ASSERT_EQUAL_FLOAT(0.2900735, bac);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_simple_unit);
  RUN_TEST(test_delete_unit);
  RUN_TEST(test_time_runs_out_add_unit);
  return UNITY_END();
}
