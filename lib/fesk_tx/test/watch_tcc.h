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
    // 4-FSK tones
    BUZZER_NOTE_D7 = 1,               // 2349.32 Hz (dibit 00)
    BUZZER_NOTE_E7 = 2,               // 2637.02 Hz (dibit 01)
    BUZZER_NOTE_F7SHARP_G7FLAT = 3,   // 2959.96 Hz (dibit 10)
    BUZZER_NOTE_G7SHARP_A7FLAT = 4,   // 3322.44 Hz (dibit 11)
} watch_buzzer_note_t;

#endif // WATCH_TCC_H
