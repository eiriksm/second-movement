/*
 * FESK Tone Sequence Demo
 * Shows the exact sequence of tones that will be transmitted for given text
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fesk_tx.h"

// State names for debugging
static const char* state_names[] = {
    "PREAMBLE", "SYNC", "HEADER", "PAYLOAD", "CRC", "COMPLETE"
};

// Tone names for readability
static const char* tone_names[] = {"f0", "f1", "f2"};
static const uint16_t tone_freqs[] = {2400, 3000, 3600};

static void print_protocol_info() {
    printf("=== HT3 Protocol Info ===\n");
    printf("Frequencies: f0=%dHz, f1=%dHz, f2=%dHz (4:5:6 ratio)\n", 
           tone_freqs[0], tone_freqs[1], tone_freqs[2]);
    printf("Symbol duration: 93.75ms (6 ticks @ 64Hz)\n");
    printf("Modulation: 3-FSK non-coherent\n");
    printf("Frame structure: [PREAMBLE] [SYNC] [HEADER] [PAYLOAD] [CRC]\n\n");
}

static void analyze_transmission(const uint8_t* text, size_t len) {
    printf("=== Analyzing transmission for: \"%.*s\" (%zu bytes) ===\n\n", (int)len, text, len);
    
    fesk_encoder_state_t encoder;
    fesk_result_t result = fesk_init_encoder(&encoder, text, len);
    
    if (result != FESK_SUCCESS) {
        printf("ERROR: Failed to initialize encoder (result %d)\n", result);
        return;
    }
    
    printf("Tone Sequence:\n");
    printf("Pos  State      Tone  Freq   Description\n");
    printf("---  ---------  ----  ----   -----------\n");
    
    int pos = 0;
    int state_start_pos[6] = {0}; // Track where each state starts
    int current_state = -1;
    uint8_t tone;
    
    while ((tone = fesk_get_next_tone(&encoder)) != 255 && pos < 1000) {
        // Track state transitions
        if (encoder.state != current_state) {
            if (current_state >= 0 && current_state < 6) {
                printf("     [%s ends at position %d, %d tones]\n", 
                       state_names[current_state], pos-1, pos - state_start_pos[current_state]);
            }
            current_state = encoder.state;
            if (current_state < 6) {
                state_start_pos[current_state] = pos;
                printf("     [%s starts]\n", state_names[current_state]);
            }
        }
        
        printf("%3d  %-9s  %-4s  %4d   ", pos, 
               current_state < 6 ? state_names[current_state] : "UNKNOWN",
               tone < 3 ? tone_names[tone] : "???",
               tone < 3 ? tone_freqs[tone] : 0);
               
        // Add contextual information
        if (current_state == 0 && pos < 12) { // PREAMBLE
            printf("Preamble bit %d = %s", pos, pos % 2 == 0 ? "1" : "0");
        } else if (current_state == 1) { // SYNC
            printf("Barker-13 sync bit %d", pos - state_start_pos[1]);
        } else if (current_state == 2) { // HEADER
            printf("Header (payload length = %d bytes)", len);
        } else if (current_state == 3) { // PAYLOAD
            printf("Payload data");
        } else if (current_state == 4) { // CRC
            printf("CRC-16 checksum");
        }
        
        printf("\n");
        pos++;
    }
    
    if (current_state >= 0 && current_state < 6) {
        printf("     [%s ends, %d tones total]\n", 
               state_names[current_state], pos - state_start_pos[current_state]);
    }
    
    printf("\n=== Summary ===\n");
    printf("Total tones: %d\n", pos);
    printf("Total duration: %.2f seconds\n", pos * 0.09375); // 93.75ms per tone
    printf("Data throughput: %.1f bps (including overhead)\n", 
           (len * 8.0) / (pos * 0.09375));
    printf("Protocol efficiency: %.1f%% (data vs total bits transmitted)\n",
           (len * 8.0) / (pos * 1.585) * 100); // 1.585 bits per trit
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s \"text to transmit\"\n", argv[0]);
        printf("Examples:\n");
        printf("  %s \"test\"\n", argv[0]);
        printf("  %s \"Hello World!\"\n", argv[0]);
        return 1;
    }
    
    const char* text = argv[1];
    size_t len = strlen(text);
    
    if (len == 0) {
        printf("ERROR: Empty text\n");
        return 1;
    }
    
    if (len > 256) {
        printf("ERROR: Text too long (max 256 bytes)\n");
        return 1;
    }
    
    print_protocol_info();
    analyze_transmission((const uint8_t*)text, len);
    
    return 0;
}