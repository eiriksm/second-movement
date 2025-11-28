/*
 * MIT License
 *
 * Copyright (c) 2025
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "ir_command_face.h"
#include "filesystem.h"
#include "lfs.h"

#include "uart.h"

// External filesystem instance from filesystem.c
extern lfs_t eeprom_filesystem;

static void list_files(ir_command_state_t *state) {
    lfs_dir_t dir;
    int err = lfs_dir_open(&eeprom_filesystem, &dir, "/");
    if (err < 0) {
        state->file_count = 0;
        return;
    }

    struct lfs_info info;
    state->file_count = 0;

    while (state->file_count < 16) {
        int res = lfs_dir_read(&eeprom_filesystem, &dir, &info);
        if (res <= 0) break;

        // Skip . and .. entries
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;

        // Only list files, not directories
        if (info.type == LFS_TYPE_REG) {
            strncpy(state->filenames[state->file_count], info.name, 12);
            state->filenames[state->file_count][12] = '\0';
            state->file_sizes[state->file_count] = info.size;
            state->file_count++;
        }
    }

    lfs_dir_close(&eeprom_filesystem, &dir);
}

static void display_file_list(ir_command_state_t *state) {
    watch_clear_display();

    if (state->file_count == 0) {
        watch_display_text(WATCH_POSITION_TOP, "no    ");
        watch_display_text(WATCH_POSITION_BOTTOM, "FILES ");
        return;
    }

    // Display current file number out of total
    char buf[11];
    snprintf(buf, 11, "%d/%d", state->current_file + 1, state->file_count);
    watch_display_text(WATCH_POSITION_TOP, buf);

    // Display filename (truncated to 6 chars if needed)
    char filename_display[7];
    strncpy(filename_display, state->filenames[state->current_file], 6);
    filename_display[6] = '\0';
    watch_display_text(WATCH_POSITION_BOTTOM, filename_display);
}

static void display_text_content(ir_command_state_t *state) {
    watch_clear_display();

    if (state->content_size == 0) {
        watch_display_text(WATCH_POSITION_TOP, "EMPty ");
        return;
    }

    // Display 6 characters at current offset
    char display[7];
    uint16_t remaining = state->content_size - state->content_offset;
    uint16_t to_show = remaining > 6 ? 6 : remaining;

    strncpy(display, state->file_content + state->content_offset, to_show);
    display[to_show] = '\0';

    // Show offset indicator on top
    char offset_str[11];
    snprintf(offset_str, 11, "%d/%d", state->content_offset, state->content_size);
    watch_display_text(WATCH_POSITION_TOP, offset_str);
    watch_display_text(WATCH_POSITION_BOTTOM, display);
}

static void execute_command(ir_command_state_t *state, const char *cmd) {
    // Parse command and arguments
    char cmd_copy[64];
    strncpy(cmd_copy, cmd, 63);
    cmd_copy[63] = '\0';

    char *command = strtok(cmd_copy, " ");
    if (!command) return;

    // Free any previous content
    if (state->file_content) {
        free(state->file_content);
        state->file_content = NULL;
        state->content_size = 0;
        state->content_offset = 0;
    }

    if (strcmp(command, "ls") == 0) {
        movement_force_led_on(0, 48, 0);  // Green LED for success
        list_files(state);
        state->current_file = 0;
        state->display_mode = true;
        display_file_list(state);

    } else if (strcmp(command, "cat") == 0) {
        char *filename = strtok(NULL, " ");
        if (!filename) {
            movement_force_led_on(48, 0, 0);  // Red LED for error
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "Cat   ");
            watch_display_text(WATCH_POSITION_BOTTOM, "no FiL");
            return;
        }

        // Read file from filesystem
        lfs_file_t file;
        int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDONLY);
        if (err < 0) {
            movement_force_led_on(48, 0, 0);  // Red LED for error
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "not   ");
            watch_display_text(WATCH_POSITION_BOTTOM, "Found ");
            return;
        }

        lfs_soff_t size = lfs_file_size(&eeprom_filesystem, &file);
        if (size > 0 && size < 4096) {  // Limit to 4KB
            state->file_content = malloc(size + 1);
            if (state->file_content) {
                lfs_file_read(&eeprom_filesystem, &file, state->file_content, size);
                state->file_content[size] = '\0';
                state->content_size = size;
                state->content_offset = 0;
                state->display_mode = true;
                movement_force_led_on(0, 48, 0);  // Green LED
                display_text_content(state);
            } else {
                movement_force_led_on(48, 0, 0);  // Red LED - no memory
                watch_clear_display();
                watch_display_text(WATCH_POSITION_TOP, "no    ");
                watch_display_text(WATCH_POSITION_BOTTOM, "MEMory");
            }
        } else {
            movement_force_led_on(48, 48, 0);  // Yellow LED - file too big
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "FiLE  ");
            watch_display_text(WATCH_POSITION_BOTTOM, "tooBig");
        }
        lfs_file_close(&eeprom_filesystem, &file);

    } else if (strcmp(command, "echo") == 0) {
        char *text = strtok(NULL, "");  // Get rest of string
        if (!text) {
            movement_force_led_on(48, 48, 0);  // Yellow LED
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "ECHo  ");
            watch_display_text(WATCH_POSITION_BOTTOM, "EMPty ");
            return;
        }

        size_t len = strlen(text);
        state->file_content = malloc(len + 1);
        if (state->file_content) {
            strcpy(state->file_content, text);
            state->content_size = len;
            state->content_offset = 0;
            state->display_mode = true;
            movement_force_led_on(0, 48, 0);  // Green LED
            display_text_content(state);
        }

    } else if (strcmp(command, "df") == 0) {
        // Display filesystem usage
        struct lfs_fsstat fsstat;
        int err = lfs_fs_stat(&eeprom_filesystem, &fsstat);
        if (err >= 0) {
            // Calculate used and total blocks
            uint32_t total_kb = (fsstat.block_count * fsstat.block_size) / 1024;
            // LittleFS doesn't track used blocks directly, approximate by counting files
            uint32_t used_kb = 0;

            lfs_dir_t dir;
            if (lfs_dir_open(&eeprom_filesystem, &dir, "/") >= 0) {
                struct lfs_info info;
                while (lfs_dir_read(&eeprom_filesystem, &dir, &info) > 0) {
                    if (info.type == LFS_TYPE_REG) {
                        used_kb += (info.size + 1023) / 1024;  // Round up to KB
                    }
                }
                lfs_dir_close(&eeprom_filesystem, &dir);
            }

            // Create display string
            char df_text[64];
            snprintf(df_text, 64, "%luK/%luK", (unsigned long)used_kb, (unsigned long)total_kb);

            size_t len = strlen(df_text);
            state->file_content = malloc(len + 1);
            if (state->file_content) {
                strcpy(state->file_content, df_text);
                state->content_size = len;
                state->content_offset = 0;
                state->display_mode = true;
                movement_force_led_on(0, 48, 0);  // Green LED
                display_text_content(state);
            }
        } else {
            movement_force_led_on(48, 0, 0);  // Red LED
            watch_clear_display();
            watch_display_text(WATCH_POSITION_TOP, "FS    ");
            watch_display_text(WATCH_POSITION_BOTTOM, "Error ");
        }

    } else {
        // Unknown command
        movement_force_led_on(48, 48, 0);  // Yellow LED for unknown
        watch_clear_display();
        watch_display_text(WATCH_POSITION_TOP, "UnKno ");
        watch_display_text(WATCH_POSITION_BOTTOM, "Wn Cmd");
    }
}

void ir_command_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(ir_command_state_t));
        memset(*context_ptr, 0, sizeof(ir_command_state_t));
    }
}

void ir_command_face_activate(void *context) {
    ir_command_state_t *state = (ir_command_state_t *)context;
    state->display_mode = false;
    state->current_file = 0;
    state->file_count = 0;

#ifdef HAS_IR_SENSOR
    // Initialize IR receiver on hardware
    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();
    HAL_GPIO_IRSENSE_in();
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_SERCOM_ALT);
#endif
    // Initialize UART (works in both hardware and simulator)
    uart_init_instance(0, UART_TXPO_NONE, UART_RXPO_0, 900);
    uart_set_irda_mode_instance(0, true);
    uart_enable_instance(0);
}

bool ir_command_face_loop(movement_event_t event, void *context) {
    ir_command_state_t *state = (ir_command_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_NONE:
            watch_clear_display();
            if (state->display_mode) {
                display_file_list(state);
            } else {
                watch_display_text(WATCH_POSITION_TOP, "IR    ");
                watch_display_text(WATCH_POSITION_BOTTOM, "Cmd   ");
            }
            break;

        case EVENT_TICK:
        {
            // Read from UART (works in both hardware and simulator)
            char data[64];
            size_t bytes_read = uart_read_instance(0, data, 63);

            if (bytes_read > 0) {
                data[bytes_read] = '\0';

                // Trim whitespace/newlines
                while (bytes_read > 0 && (data[bytes_read-1] == '\n' || data[bytes_read-1] == '\r' || data[bytes_read-1] == ' ')) {
                    data[bytes_read-1] = '\0';
                    bytes_read--;
                }

                if (bytes_read > 0) {
                    execute_command(state, data);
                }
            } else {
                movement_force_led_off();
                if (!state->display_mode) {
                    // Blink indicator to show we're waiting
                    if (watch_rtc_get_date_time().unit.second % 2 == 0) {
                        watch_set_indicator(WATCH_INDICATOR_SIGNAL);
                    } else {
                        watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
                    }
                }
            }
        }
            break;

        case EVENT_LIGHT_BUTTON_UP:
            // Trigger "ls" command manually
            if (!state->display_mode) {
                execute_command(state, "ls");
            }
            break;

        case EVENT_ALARM_BUTTON_UP:
            if (state->display_mode) {
                if (state->file_count > 0 && !state->file_content) {
                    // Navigate through file list (ls mode)
                    state->current_file = (state->current_file + 1) % state->file_count;
                    display_file_list(state);
                } else if (state->file_content && state->content_size > 0) {
                    // Scroll through text content (cat/echo mode)
                    if (state->content_offset + 6 < state->content_size) {
                        state->content_offset += 6;
                        display_text_content(state);
                    } else {
                        // Wrap to beginning
                        state->content_offset = 0;
                        display_text_content(state);
                    }
                }
            }
            break;

        case EVENT_ALARM_LONG_PRESS:
            // Return to command mode
            if (state->display_mode) {
                state->display_mode = false;
                // Free any content
                if (state->file_content) {
                    free(state->file_content);
                    state->file_content = NULL;
                    state->content_size = 0;
                    state->content_offset = 0;
                }
                watch_clear_display();
                watch_display_text(WATCH_POSITION_TOP, "IR    ");
                watch_display_text(WATCH_POSITION_BOTTOM, "Cmd   ");
            }
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        case EVENT_LOW_ENERGY_UPDATE:
            watch_display_text(WATCH_POSITION_TOP_RIGHT, " <");
            break;

        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void ir_command_face_resign(void *context) {
    ir_command_state_t *state = (ir_command_state_t *)context;

    // Free any allocated content
    if (state->file_content) {
        free(state->file_content);
        state->file_content = NULL;
        state->content_size = 0;
        state->content_offset = 0;
    }

    uart_disable_instance(0);
#ifdef HAS_IR_SENSOR
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
#endif
}

// Note: irq_handler_sercom0 is defined in irda_upload_face.c
// Both faces use UART instance 0, so we share the same handler
