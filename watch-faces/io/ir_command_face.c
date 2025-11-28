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

static void list_files(void) {
    lfs_dir_t dir;
    int err = lfs_dir_open(&eeprom_filesystem, &dir, "/");
    if (err < 0) {
        printf("ir_cmd: ls: error opening directory\n");
        return;
    }

    struct lfs_info info;
    int count = 0;

    while (true) {
        int res = lfs_dir_read(&eeprom_filesystem, &dir, &info);
        if (res <= 0) break;

        // Skip . and .. entries
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;

        // Only list files, not directories
        if (info.type == LFS_TYPE_REG) {
            printf("ir_cmd: %s (%ld bytes)\n", info.name, (long)info.size);
            count++;
        }
    }

    if (count == 0) {
        printf("ir_cmd: (no files)\n");
    }

    lfs_dir_close(&eeprom_filesystem, &dir);
}

static void execute_command(ir_command_state_t *state, const char *cmd) {
    (void)state;  // Unused now

    // Parse command and arguments
    char cmd_copy[64];
    strncpy(cmd_copy, cmd, 63);
    cmd_copy[63] = '\0';

    char *command = strtok(cmd_copy, " ");
    if (!command) return;

    if (strcmp(command, "ls") == 0) {
        list_files();

    } else if (strcmp(command, "cat") == 0) {
        char *filename = strtok(NULL, " ");
        if (!filename) {
            printf("ir_cmd: cat: missing filename\n");
            return;
        }

        // Read file from filesystem
        lfs_file_t file;
        int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDONLY);
        if (err < 0) {
            printf("ir_cmd: cat: %s: not found\n", filename);
            return;
        }

        lfs_soff_t size = lfs_file_size(&eeprom_filesystem, &file);
        if (size > 0 && size < 4096) {  // Limit to 4KB
            char *buffer = malloc(size + 1);
            if (buffer) {
                lfs_file_read(&eeprom_filesystem, &file, buffer, size);
                buffer[size] = '\0';
                printf("ir_cmd: %s\n", buffer);
                free(buffer);
            } else {
                printf("ir_cmd: cat: out of memory\n");
            }
        } else if (size == 0) {
            printf("ir_cmd: (empty file)\n");
        } else {
            printf("ir_cmd: cat: file too large (%ld bytes)\n", (long)size);
        }
        lfs_file_close(&eeprom_filesystem, &file);

    } else if (strcmp(command, "echo") == 0) {
        char *rest = strtok(NULL, "");  // Get rest of string
        if (!rest) {
            printf("ir_cmd: \n");
            return;
        }

        // Check for output redirection: echo text > filename
        char *redirect = strstr(rest, " > ");
        if (redirect) {
            // Split text and filename
            *redirect = '\0';  // Terminate text part
            char *filename = redirect + 3;  // Skip " > "

            // Trim whitespace from filename
            while (*filename == ' ') filename++;

            if (*filename == '\0') {
                printf("ir_cmd: echo: missing filename after >\n");
                return;
            }

            // Write to file
            lfs_file_t file;
            int err = lfs_file_open(&eeprom_filesystem, &file, filename,
                                   LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
            if (err < 0) {
                printf("ir_cmd: echo: cannot create %s\n", filename);
                return;
            }

            lfs_file_write(&eeprom_filesystem, &file, rest, strlen(rest));
            lfs_file_close(&eeprom_filesystem, &file);
            printf("ir_cmd: wrote to %s\n", filename);
        } else {
            // Just echo to output
            printf("ir_cmd: %s\n", rest);
        }

    } else if (strcmp(command, "df") == 0) {
        // Display filesystem usage
        struct lfs_fsstat fsstat;
        int err = lfs_fs_stat(&eeprom_filesystem, &fsstat);
        if (err >= 0) {
            // Calculate used and total blocks
            uint32_t total_kb = (fsstat.block_count * fsstat.block_size) / 1024;
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

            printf("ir_cmd: %luK / %luK used\n", (unsigned long)used_kb, (unsigned long)total_kb);
        } else {
            printf("ir_cmd: df: filesystem error\n");
        }

    } else {
        printf("ir_cmd: %s: unknown command\n", command);
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
