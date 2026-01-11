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
#include <stdio.h>
#include "ir_command_face.h"
#include "filesystem.h"
#include "lfs.h"

#include "uart.h"

extern lfs_t eeprom_filesystem;

static void cmd_ls(ir_command_state_t *state) {
    lfs_dir_t dir;
    int err = lfs_dir_open(&eeprom_filesystem, &dir, "/");
    if (err < 0) {
        state->output_len += snprintf(state->output_buffer + state->output_len,
                                      512 - state->output_len, "error opening dir\n");
        return;
    }

    struct lfs_info info;
    while (lfs_dir_read(&eeprom_filesystem, &dir, &info) > 0) {
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;
        if (info.type == LFS_TYPE_REG) {
            state->output_len += snprintf(state->output_buffer + state->output_len,
                                         512 - state->output_len,
                                         "%s (%ld bytes)\n", info.name, (long)info.size);
        }
    }
    lfs_dir_close(&eeprom_filesystem, &dir);
}

static void cmd_cat(ir_command_state_t *state, const char *filename) {
    lfs_file_t file;
    int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDONLY);
    if (err < 0) {
        state->output_len += snprintf(state->output_buffer + state->output_len,
                                      512 - state->output_len, "%s: not found\n", filename);
        return;
    }

    lfs_soff_t size = lfs_file_size(&eeprom_filesystem, &file);
    if (size > 0 && size < 400) {  // Limit to fit in buffer
        int read = lfs_file_read(&eeprom_filesystem, &file,
                                state->output_buffer + state->output_len,
                                512 - state->output_len - 1);
        if (read > 0) {
            state->output_len += read;
            state->output_buffer[state->output_len++] = '\n';
        }
    } else if (size == 0) {
        state->output_len += snprintf(state->output_buffer + state->output_len,
                                      512 - state->output_len, "(empty)\n");
    } else {
        state->output_len += snprintf(state->output_buffer + state->output_len,
                                      512 - state->output_len, "file too large\n");
    }
    lfs_file_close(&eeprom_filesystem, &file);
}

static void cmd_df(ir_command_state_t *state) {
    int32_t free_bytes = filesystem_get_free_space();
    state->output_len += snprintf(state->output_buffer + state->output_len,
                                  512 - state->output_len,
                                  "free: %ld bytes\n", (long)free_bytes);
}

static void execute_command(ir_command_state_t *state, const char *cmd) {
    // Clear output buffer
    state->output_len = 0;
    state->output_buffer[0] = '\0';

    // Parse command into argc/argv format
    char cmd_copy[64];
    strncpy(cmd_copy, cmd, 63);
    cmd_copy[63] = '\0';

    // Check for echo - needs special parsing for redirect
    if (strncmp(cmd, "echo ", 5) == 0) {
        const char *text_start = cmd + 5;

        // Look for redirect operators
        const char *redirect_write = strstr(text_start, " > ");
        const char *redirect_append = strstr(text_start, " >> ");

        if (redirect_write || redirect_append) {
            // Parse for filesystem_cmd_echo format: echo TEXT > FILE
            static char text[128];
            static char op[3];
            static char filename[32];

            const char *redirect = redirect_append ? redirect_append : redirect_write;
            size_t text_len = redirect - text_start;
            strncpy(text, text_start, text_len);
            text[text_len] = '\0';

            if (redirect_append) {
                strcpy(op, ">>");
                const char *file = redirect_append + 4;
                while (*file == ' ') file++;
                strncpy(filename, file, 31);
                filename[31] = '\0';
            } else {
                strcpy(op, ">");
                const char *file = redirect_write + 3;
                while (*file == ' ') file++;
                strncpy(filename, file, 31);
                filename[31] = '\0';
            }

            char *argv[4] = {"echo", text, op, filename};
            filesystem_cmd_echo(4, argv);
            // No output for echo
        }
        // No output for plain echo either
        return;
    } else {
        // Simple tokenizer to build argv for other commands
        char *argv[10];
        int argc = 0;

        char *token = strtok(cmd_copy, " ");
        while (token && argc < 10) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        if (argc == 0) return;

        // Dispatch to custom implementations that write to buffer
        if (strcmp(argv[0], "ls") == 0) {
            cmd_ls(state);
        } else if (strcmp(argv[0], "cat") == 0 && argc > 1) {
            cmd_cat(state, argv[1]);
        } else if (strcmp(argv[0], "df") == 0) {
            cmd_df(state);
        } else {
            state->output_len += snprintf(state->output_buffer + state->output_len,
                                         512 - state->output_len,
                                         "%s: unknown command\n", argv[0]);
        }
    }

    // Print the buffered output
    if (state->output_len > 0) {
        printf("%s", state->output_buffer);
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
    (void)context;

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
            watch_display_text(WATCH_POSITION_TOP, "IR    ");
            watch_display_text(WATCH_POSITION_BOTTOM, "Cmd   ");
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
                // Blink indicator to show we're waiting for commands
                if (watch_rtc_get_date_time().unit.second % 2 == 0) {
                    watch_set_indicator(WATCH_INDICATOR_SIGNAL);
                } else {
                    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
                }
            }
        }
            break;

        case EVENT_LIGHT_BUTTON_UP:
            // Trigger "ls" command manually
            execute_command(state, "ls");
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
    (void)context;

    uart_disable_instance(0);
#ifdef HAS_IR_SENSOR
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
#endif
}

// Note: irq_handler_sercom0 is defined in irda_upload_face.c
// Both faces use UART instance 0, so we share the same handler
