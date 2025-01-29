#ifndef WATCH_H
#define WATCH_H

#include <stdint.h>

typedef union {
    struct {
        uint32_t second : 6;    // 0-59
        uint32_t minute : 6;    // 0-59
        uint32_t hour : 5;      // 0-23
        uint32_t day : 5;       // 1-31
        uint32_t month : 4;     // 1-12
        uint32_t year : 6;      // 0-63 (representing 2020-2083)
    } unit;
    uint32_t reg;               // the bit-packed value as expected by the RTC peripheral's CLOCK register.
} rtc_date_time_t;

#define watch_date_time_t rtc_date_time_t

/// An enum listing the locations on the display where text can be placed.
typedef enum {
    WATCH_POSITION_FULL = 0,    ///< Display 10 characters to the full screen, in the standard F-91W layout.
    WATCH_POSITION_TOP,         ///< Display 2 (classic) or 5 (custom) characters at the top of the screen. On custom LCD, overwrites top right positon.
    WATCH_POSITION_TOP_LEFT,    ///< Display 2 or 3 characters in the top left of the screen.
    WATCH_POSITION_TOP_RIGHT,   ///< Display 2 digits in the top right of the screen.
    WATCH_POSITION_BOTTOM,      ///< Display 6 characters at the bottom of the screen, the main line.
    WATCH_POSITION_HOURS,       ///< Display 2 characters in the hours portion of the main line.
    WATCH_POSITION_MINUTES,     ///< Display 2 characters in the minutes portion of the main line.
    WATCH_POSITION_SECONDS,     ///< Display 2 characters in the seconds portion of the main line.
} watch_position_t;

void watch_set_led_off(void);
void watch_display_text(watch_position_t location, const char *string);

void watch_display_text_with_fallback(watch_position_t location, const char *string, const char *fallback);

uint32_t watch_utility_date_time_to_unix_time(watch_date_time_t date_time, int32_t utc_offset);


#endif // WATCH_H
