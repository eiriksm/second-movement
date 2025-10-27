/*
 * MIT License
 *
 * Copyright (c) 2024 Second Movement Project
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

#ifndef FESK_DEMO_FACE_H_
#define FESK_DEMO_FACE_H_

/*
 * FESK DEMO FACE
 * 
 * A simple demonstration of the Harmonic Triad 3-FSK Acoustic Protocol (HT3).
 * 
 * This face demonstrates the fesk_tx protocol by transmitting the phrase "hello from fesk"
 * when the alarm button is pressed. Features a countdown before transmission
 * similar to the chirpy demo.
 * 
 * Usage:
 * - ALARM button: Start transmission of "test"
 * - MODE button: Exit to next face
 * 
 * The transmission uses the HT3 protocol with 4:5:6 major triad frequencies
 * for pleasant, harmonious acoustic data transmission.
 */

#include "movement.h"

void fesk_demo_face_setup(uint8_t watch_face_index, void **context_ptr);
void fesk_demo_face_activate(void *context);
bool fesk_demo_face_loop(movement_event_t event, void *context);
void fesk_demo_face_resign(void *context);

#define fesk_demo_face ((watch_face_t){ \
    fesk_demo_face_setup, \
    fesk_demo_face_activate, \
    fesk_demo_face_loop, \
    fesk_demo_face_resign, \
    NULL, \
})

#endif // FESK_DEMO_FACE_H_
