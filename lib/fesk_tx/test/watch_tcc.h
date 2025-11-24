/*
 * Mock watch_tcc.h for testing
 * Provides minimal definitions needed for fesk_tx unit tests
 */

#ifndef WATCH_TCC_H
#define WATCH_TCC_H

#include <stdint.h>

// Mock buzzer note type
typedef enum {
    BUZZER_NOTE_REST = 0,
    // 8-FSK tones
    BUZZER_NOTE_C7 = 1,                  // 2093.00 Hz (tribit 000)
    BUZZER_NOTE_C7SHARP_D7FLAT = 2,      // 2217.46 Hz (tribit 001)
    BUZZER_NOTE_D7 = 3,                  // 2349.32 Hz (tribit 010)
    BUZZER_NOTE_D7SHARP_E7FLAT = 4,      // 2489.02 Hz (tribit 011)
    BUZZER_NOTE_E7 = 5,                  // 2637.02 Hz (tribit 100)
    BUZZER_NOTE_F7 = 6,                  // 2793.83 Hz (tribit 101)
    BUZZER_NOTE_F7SHARP_G7FLAT = 7,      // 2959.96 Hz (tribit 110)
    BUZZER_NOTE_G7 = 8,                  // 3135.96 Hz (tribit 111)
} watch_buzzer_note_t;

#endif // WATCH_TCC_H
