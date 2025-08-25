# Pentatonic Transmission Library - Testing

This document describes the testing framework for the pentatonic audio transmission library.

## Test Suite Overview

The testing framework includes:
1. **Unit tests** - Comprehensive testing of all library functions
2. **Text-to-tones demo** - Interactive demonstration of text encoding to audio tones
3. **Performance tests** - Benchmarking and profiling tools
4. **Memory tests** - Leak detection and memory safety validation

## Quick Start

```bash
# Build and run all tests
make -f Makefile.test

# Run just unit tests
make -f Makefile.test run-test

# Run interactive demo
make -f Makefile.test run-demo

# Test custom text
make -f Makefile.test demo-text TEXT="Your message here"
```

## Unit Tests

### Test Coverage

- ✅ **CRC-8 validation** - Ensures data integrity checking works correctly
- ✅ **Reed-Solomon encoding/decoding** - Tests forward error correction
- ✅ **Encoder initialization** - Validates configuration and setup
- ✅ **Text-to-tone conversion** - Core functionality testing
- ✅ **Block repetition** - Tests redundancy mechanisms 
- ✅ **Frequency spacing** - Validates audio separation for reliability
- ✅ **Sync patterns** - Tests timing recovery features
- ✅ **Calibration sequence** - Tests automatic hardware clock compensation
- ✅ **Calibration helpers** - Tests frequency multiplier calculation for RX
- ✅ **Configuration validation** - Parameter boundary testing

### Example Output

```
=== Pentatonic Transmission Library Unit Tests ===

PASS: test_crc8_basic
PASS: test_reed_solomon_basic  
PASS: test_encoder_init
Text 'Hello World!' -> 92 tones: 0 0 0 0 0 0 0 0 9 9 9 5 5 5 5 5 5 5 5 9 ...
PASS: test_text_to_tones
PASS: test_block_repetition
PASS: test_frequency_spacing
PASS: test_sync_patterns
PASS: test_calibration_sequence
PASS: test_calibration_helpers
PASS: test_config_validation

=== Test Results ===
Passed: 10/10
All tests PASSED! ✓
```

## Text-to-Tones Demo

### Features

- **Multiple reliability modes** - Compare speed vs reliability tradeoffs
- **Musical representation** - See tones as actual musical notes
- **Frequency analysis** - Understand audio characteristics
- **Interactive mode** - Test custom messages
- **Detailed statistics** - Transmission efficiency metrics

### Example: "SOS" Message

```
Input text: "SOS" (3 bytes)

### Balanced Mode ###
Config: 16-byte blocks, 2x repetition, enhanced encoding
First 20 tones: 0 1 2 3 9 9 8 9 8 8 0 0 0 0 8 1 1 0 3 1 ...
Musical notes:  E4  C#5 A5  E6  --  --  C#7 --  C#7 C#7 E4  E4  E4  E4  C#7 C#5 C#5 E4  E6  C#5 ...
Stats: 1 blocks sent, 1 retransmitted, 33 total tones
Efficiency: 11.0 tones per input byte
```

### Reliability Mode Comparison

| Mode | Speed | Redundancy | Efficiency | Use Case |
|------|-------|------------|------------|----------|
| **Speed Priority** | ~45 bps | 1x blocks | 5-7 tones/byte | Fast, clean channels |
| **Balanced** | ~30 bps | 2x blocks | 8-11 tones/byte | General purpose |  
| **Reliability Priority** | ~20 bps | 3x blocks + RS | 20-34 tones/byte | Noisy environments |
| **Musical Mode** | ~25 bps | 2x blocks | 8-11 tones/byte | Pleasant sounding |

### Frequency Analysis

The demo shows how text gets converted to specific frequencies:

**Enhanced Encoding (Recommended):**
- **Tone 0 (E4)**: 330Hz - Low baseline
- **Tone 1 (C#5)**: 550Hz - +220Hz gap  
- **Tone 2 (A5)**: 880Hz - +330Hz gap
- **Tone 3 (E6)**: 1320Hz - +440Hz gap
- **Control (C#7)**: 2200Hz - Very high for framing
- **Silence**: No tone

This wide frequency spacing (220-440Hz gaps) ensures reliable audio transmission even with background noise.

## Advanced Testing

### Memory Testing

```bash
# Requires valgrind
make -f Makefile.test test-memory
```

### Code Coverage

```bash  
# Requires gcov
make -f Makefile.test test-coverage
```

### Performance Benchmarking

```bash
make -f Makefile.test test-perf
```

## Key Improvements Verified by Tests

### 1. Fixed Critical Bugs
- ✅ **CRC accumulation bug** - Now properly accumulates instead of overwriting
- ✅ **Global state race conditions** - Each encoder has isolated state  
- ✅ **Buffer overflow handling** - Proper error returns vs silent failures
- ✅ **Reed-Solomon integration** - Complete FEC implementation

### 2. Enhanced Reliability  
- ✅ **Block repetition** - 1-3x redundancy based on mode
- ✅ **Sync patterns** - Timing recovery every 4th block
- ✅ **Wide frequency spacing** - 220-440Hz gaps for better separation
- ✅ **Triple voting** - Majority vote error correction

### 3. One-Way Audio Optimizations
- ✅ **No pointless retries** - Focused on redundancy strategies
- ✅ **Timing recovery** - Sync tones for receiver synchronization  
- ✅ **Musical framing** - Pleasant start/end sequences
- ✅ **Adaptive redundancy** - Mode-specific repetition strategies

## Integration with Movement

The library is designed to integrate cleanly with the Movement watch framework:

```c
#include "pentatonic_tx.h"

// Your data callback
uint8_t get_watch_data(uint8_t *byte) {
    // Return watch data (time, activity, etc.)
    return get_next_transmission_byte(byte);
}

// Initialize for balanced reliability
penta_encoder_state_t encoder;
penta_init_encoder(&encoder, get_watch_data, NULL);

// In your 64Hz update loop:
uint8_t tone = penta_get_next_tone(&encoder);
if (tone != 255) {
    uint16_t period = penta_get_tone_period_for_encoder(&encoder, tone);
    if (period > 0) {
        watch_set_buzzer_period_and_duty_cycle(period, 25);
        watch_set_buzzer_on();
    } else {
        watch_set_buzzer_off(); // Silence
    }
} else {
    watch_set_buzzer_off(); // Transmission complete
}
```

## Test File Structure

```
lib/pentatonic_tx/
├── test_pentatonic.c      # Unit tests
├── demo_text_to_tones.c   # Interactive demo  
├── Makefile.test          # Test build system
├── TESTING.md            # This documentation
└── ...                   # Library source files
```

The testing framework provides comprehensive validation of the library's reliability improvements and demonstrates practical usage for text-to-audio transmission.