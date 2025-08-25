/*
 * Demo: Convert text to pentatonic tone sequences
 * 
 * This demo shows how text gets converted to musical tones for transmission.
 * It prints both the raw tone sequence and a visual representation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pentatonic_tx.h"

// Demo text data
static const char* demo_text = "Hello";
static int text_pos = 0;

// Data provider for demo
static uint8_t get_demo_byte(uint8_t *next_byte) {
    if (text_pos < strlen(demo_text)) {
        *next_byte = (uint8_t)demo_text[text_pos];
        text_pos++;
        return 1;
    }
    return 0;
}

// Convert tone to musical note name
static const char* tone_to_note(uint8_t tone, bool enhanced) {
    if (enhanced) {
        switch (tone) {
            case 0: return "E4 ";
            case 1: return "C#5";  
            case 2: return "A5 ";
            case 3: return "E6 ";
            case 4: return "E4*";
            case 5: return "A5*";
            case 6: return "E6*";
            case 7: return "A6*";
            case 8: return "C#7";  // Control
            case 9: return "-- ";   // Silence
            default: return "?? ";
        }
    } else {
        switch (tone) {
            case 0: return "A4 ";
            case 1: return "B4 ";
            case 2: return "C#5";
            case 3: return "E5 ";
            case 4: return "F#5";
            case 5: return "A5 ";
            case 6: return "B5 ";
            case 7: return "C#6";
            case 8: return "E6 ";   // Control
            case 9: return "-- ";   // Silence
            default: return "?? ";
        }
    }
}

// Print tone sequence analysis
static void analyze_tone_sequence(uint8_t *tones, int count, bool enhanced) {
    printf("\n=== Tone Sequence Analysis ===\n");
    printf("Total tones: %d\n\n", count);
    
    // Print in groups of 10
    for (int i = 0; i < count; i += 10) {
        printf("Tones %3d-%3d: ", i, (i + 9 < count) ? i + 9 : count - 1);
        for (int j = 0; j < 10 && (i + j) < count; j++) {
            printf("%d ", tones[i + j]);
        }
        printf("\n");
        
        printf("Notes        : ");
        for (int j = 0; j < 10 && (i + j) < count; j++) {
            printf("%s ", tone_to_note(tones[i + j], enhanced));
        }
        printf("\n");
        
        printf("Freq (Hz)    : ");
        for (int j = 0; j < 10 && (i + j) < count; j++) {
            uint16_t freq = enhanced ? 
                (tones[i+j] == 0 ? 330 : tones[i+j] == 1 ? 550 : tones[i+j] == 2 ? 880 : 
                 tones[i+j] == 3 ? 1320 : tones[i+j] == 8 ? 2200 : 0) :
                penta_get_tone_frequency(tones[i + j]);
            if (freq > 0) {
                printf("%4d ", freq);
            } else {
                printf("---- ");
            }
        }
        printf("\n\n");
    }
    
    // Statistics
    int control_tones = 0, silence_tones = 0, data_tones = 0;
    for (int i = 0; i < count; i++) {
        if (tones[i] == 8) control_tones++;
        else if (tones[i] == 9) silence_tones++;
        else data_tones++;
    }
    
    printf("Statistics:\n");
    printf("  Data tones: %d (%.1f%%)\n", data_tones, 100.0 * data_tones / count);
    printf("  Control tones: %d (%.1f%%)\n", control_tones, 100.0 * control_tones / count);
    printf("  Silence tones: %d (%.1f%%)\n", silence_tones, 100.0 * silence_tones / count);
}

// Demo different reliability levels
static void demo_reliability_levels(const char* text) {
    printf("=== Text-to-Tone Conversion Demo ===\n");
    printf("Input text: \"%s\" (%lu bytes)\n\n", text, strlen(text));
    
    penta_reliability_level_t levels[] = {
        PENTA_SPEED_PRIORITY,
        PENTA_BALANCED, 
        PENTA_RELIABILITY_PRIORITY,
        PENTA_MUSICAL_MODE
    };
    
    const char* level_names[] = {
        "Speed Priority",
        "Balanced",
        "Reliability Priority", 
        "Musical Mode"
    };
    
    for (int level_idx = 0; level_idx < 4; level_idx++) {
        printf("### %s Mode ###\n", level_names[level_idx]);
        
        penta_encoder_state_t encoder;
        penta_config_t config;
        penta_get_default_config(levels[level_idx], &config);
        
        printf("Config: %d-byte blocks, %dx repetition, %s encoding\n",
               config.block_size, config.block_repetitions,
               config.use_enhanced_encoding ? "enhanced" : "original");
        
        // Reset text position
        demo_text = text;
        text_pos = 0;
        
        penta_result_t result = penta_init_encoder_with_config(&encoder, &config, get_demo_byte, NULL);
        if (result != PENTA_SUCCESS) {
            printf("ERROR: Failed to initialize encoder\n");
            continue;
        }
        
        // Generate tone sequence
        uint8_t tones[1000];
        int tone_count = 0;
        uint8_t tone;
        
        while ((tone = penta_get_next_tone(&encoder)) != 255 && tone_count < 1000) {
            tones[tone_count++] = tone;
        }
        
        // Print first 20 tones
        printf("First 20 tones: ");
        for (int i = 0; i < 20 && i < tone_count; i++) {
            printf("%d ", tones[i]);
        }
        if (tone_count > 20) printf("...");
        printf("\n");
        
        // Show musical representation of first 20 tones
        printf("Musical notes:  ");
        for (int i = 0; i < 20 && i < tone_count; i++) {
            printf("%s ", tone_to_note(tones[i], config.use_enhanced_encoding));
        }
        if (tone_count > 20) printf("...");
        printf("\n");
        
        // Show statistics
        const penta_stats_t* stats = penta_get_stats(&encoder);
        printf("Stats: %d blocks sent, %d retransmitted, %d total tones\n", 
               stats->blocks_sent, stats->blocks_retransmitted, tone_count);
        printf("Efficiency: %.1f tones per input byte\n", (float)tone_count / strlen(text));
        printf("\n");
        
        // Detailed analysis for first mode only
        if (level_idx == 1) { // Balanced mode
            analyze_tone_sequence(tones, tone_count, config.use_enhanced_encoding);
        }
    }
}

// Interactive mode
static void interactive_mode(void) {
    char input[256];
    printf("\n=== Interactive Mode ===\n");
    printf("Enter text to convert to tones (or 'quit' to exit):\n");
    
    while (1) {
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) break;
        
        // Remove newline
        char* newline = strchr(input, '\n');
        if (newline) *newline = '\0';
        
        if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0) {
            break;
        }
        
        if (strlen(input) == 0) continue;
        
        demo_reliability_levels(input);
    }
}

int main(int argc, char* argv[]) {
    printf("Pentatonic Transmission Library - Text to Tones Demo\n");
    printf("====================================================\n\n");
    
    if (argc > 1) {
        // Use command line argument as text
        demo_reliability_levels(argv[1]);
    } else {
        // Demo with default text, then interactive mode
        demo_reliability_levels("Hello");
        interactive_mode();
    }
    
    printf("Demo complete!\n");
    return 0;
}