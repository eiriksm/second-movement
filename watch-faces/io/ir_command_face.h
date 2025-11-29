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

#pragma once

#include "movement.h"

/*
 * IR COMMAND FACE
 *
 * Watch face that receives IR/UART commands and outputs results via printf.
 * Results are sent back through UART (visible in simulator console or serial output).
 *
 * Supported commands:
 * - ls                     : List files
 * - cat <filename>         : Display file contents
 * - echo <text>            : Echo text to output (quotes optional)
 * - echo "text" > <file>   : Write text to file (quotes optional)
 * - df                     : Show filesystem usage
 *
 * Controls:
 * - LIGHT button: Manually trigger "ls" command
 * - Watch display: Shows "IR Cmd" (output goes to UART, not display)
 *
 * IR/UART: Receives commands via IR sensor (hardware) or UART simulator
 * In simulator mode: Use the IrDA/UART upload UI in shell.html to send commands
 *                    Output appears in browser console
 */

typedef struct {
    char output_buffer[512];  // Buffer to store command output
    size_t output_len;
} ir_command_state_t;

void ir_command_face_setup(uint8_t watch_face_index, void ** context_ptr);
void ir_command_face_activate(void *context);
bool ir_command_face_loop(movement_event_t event, void *context);
void ir_command_face_resign(void *context);

#define ir_command_face ((const watch_face_t){ \
    ir_command_face_setup, \
    ir_command_face_activate, \
    ir_command_face_loop, \
    ir_command_face_resign, \
    NULL, \
})
