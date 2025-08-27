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

#ifndef PENTATONIC_TX_FACE_H_
#define PENTATONIC_TX_FACE_H_

#include "movement.h"

/** @brief Pentatonic Transmission Face
 * @details A watch face that demonstrates the pentatonic data transmission protocol.
 *          Users can select different data sources and reliability levels, then
 *          transmit the data using pleasant pentatonic scale frequencies.
 */

void pentatonic_tx_face_setup(uint8_t watch_face_index, void **context_ptr);
void pentatonic_tx_face_activate(void *context);
bool pentatonic_tx_face_loop(movement_event_t event, void *context);
void pentatonic_tx_face_resign(void *context);

#define pentatonic_tx_face ((const watch_face_t){ \
    pentatonic_tx_face_setup, \
    pentatonic_tx_face_activate, \
    pentatonic_tx_face_loop, \
    pentatonic_tx_face_resign, \
    NULL, \
})

#endif // PENTATONIC_TX_FACE_H_


