# Pentatonic Transmission - Automatic Calibration Protocol

## Overview

This document describes the **automatic calibration sequence** that the pentatonic transmission library sends at the start of every transmission. This sequence allows RX implementations (especially web apps) to detect and compensate for hardware clock rate differences between simulator and real hardware.

## Problem Statement

**Simulator vs Hardware Clock Differences:**
- Simulator runs at exactly 1.000000 MHz
- Real hardware may run at 0.997 MHz, 1.003 MHz, etc. (±0.1% to ±3% variation)
- This causes proportional frequency shifts in all transmitted tones
- RX tuned for simulator frequencies will fail on real hardware

## Solution: Automatic Calibration

The library **automatically sends a calibration sequence** before any data transmission. The RX can measure the actual frequencies and calculate a correction multiplier.

## Calibration Sequence Format

### Sequence Structure (23 tones total)

```
Phase 0-7:   A4 A4 A4 A4 A4 A4 A4 A4     (8x A4 tone - 440Hz base frequency)
Phase 8-10:  -- -- --                    (3x silence gap)
Phase 11-18: A5 A5 A5 A5 A5 A5 A5 A5     (8x A5 tone - 880Hz base frequency, perfect octave)
Phase 19-21: -- -- --                    (3x silence gap)
Phase 22:    CTRL                         (1x control tone - marks end of calibration)
```

### Frequency Mapping

**Original Encoding:**
- A4 (tone 0): 440 Hz
- A5 (tone 5): 880 Hz (perfect 2:1 octave ratio)
- Control: 1320 Hz

**Enhanced Encoding (recommended):**
- A4 (tone 0): 330 Hz  
- A5 (tone 5): 880 Hz (consistent 880Hz in both encodings)
- Control: 2200 Hz

## RX Implementation Guide

### Step 1: Detect Calibration Sequence

```javascript
// RX Web App - Detection
function detectCalibrationSequence(toneBuffer) {
    const expectedSequence = [
        0,0,0,0,0,0,0,0,  // 8x A4
        9,9,9,            // 3x silence  
        5,5,5,5,5,5,5,5,  // 8x A5
        9,9,9,            // 3x silence
        8                 // 1x control
    ];
    
    if (toneBuffer.length < 23) return false;
    
    for (let i = 0; i < 23; i++) {
        if (toneBuffer[i] !== expectedSequence[i]) {
            return false;
        }
    }
    
    return true; // Calibration sequence detected
}
```

### Step 2: Measure Actual Frequencies

```javascript
// RX Web App - Frequency Measurement  
function measureCalibrationFrequencies(audioData, sampleRate, useEnhanced) {
    // Expected frequencies
    const expectedA4 = useEnhanced ? 330 : 440;
    const expectedA5 = useEnhanced ? 880 : 880;
    
    // Measure A4 frequency (samples 0-7 of tone sequence)
    const a4Samples = extractToneSamples(audioData, 0, 8); // First 8 tones
    const measuredA4 = detectPrimaryFrequency(a4Samples, sampleRate);
    
    // Measure A5 frequency (samples 11-18 of tone sequence)  
    const a5Samples = extractToneSamples(audioData, 11, 8); // A5 tones
    const measuredA5 = detectPrimaryFrequency(a5Samples, sampleRate);
    
    return { measuredA4, measuredA5, expectedA4, expectedA5 };
}
```

### Step 3: Calculate Frequency Multiplier

```javascript
// RX Web App - Multiplier Calculation
function calculateFrequencyMultiplier(measured, expected) {
    const multiplierA4 = measured.measuredA4 / expected.expectedA4;
    const multiplierA5 = measured.measuredA5 / expected.expectedA5;
    
    // Validate consistency (both should be very close)
    const ratio = multiplierA4 / multiplierA5;
    if (ratio < 0.95 || ratio > 1.05) {
        console.warn('Calibration measurements inconsistent:', multiplierA4, multiplierA5);
        return null; // Measurement error
    }
    
    // Return average for best accuracy
    const multiplier = (multiplierA4 + multiplierA5) / 2.0;
    
    console.log(`Clock rate detected: ${(multiplier * 100).toFixed(3)}% of nominal`);
    return multiplier;
}
```

### Step 4: Apply Correction to All Subsequent Detection

```javascript
// RX Web App - Apply Calibration
class PentaReceiver {
    constructor() {
        this.frequencyMultiplier = 1.0; // Default (no correction)
        this.calibrated = false;
    }
    
    // Call this with calibration results
    setFrequencyMultiplier(multiplier) {
        this.frequencyMultiplier = multiplier;
        this.calibrated = true;
        console.log(`RX calibrated: ${multiplier.toFixed(6)}x frequency correction`);
    }
    
    // Use corrected frequencies for all tone detection
    getExpectedFrequency(toneIndex, useEnhanced) {
        const baseFreq = useEnhanced ? 
            [330, 550, 880, 1320][toneIndex] :  // Enhanced
            [440, 495, 554, 660, 740, 880, 990, 1108, 1320][toneIndex]; // Original
            
        return baseFreq * this.frequencyMultiplier;
    }
    
    // Tone detection with calibration applied
    detectTone(audioSample, sampleRate) {
        if (!this.calibrated) {
            throw new Error('RX not calibrated - process calibration sequence first');
        }
        
        const detectedFreq = this.detectPrimaryFrequency(audioSample, sampleRate);
        
        // Compare against calibrated expected frequencies
        for (let tone = 0; tone < 10; tone++) {
            const expectedFreq = this.getExpectedFrequency(tone, this.useEnhanced);
            const tolerance = expectedFreq * 0.05; // 5% tolerance
            
            if (Math.abs(detectedFreq - expectedFreq) < tolerance) {
                return tone;
            }
        }
        
        return -1; // No matching tone found
    }
}
```

## Example Calibration Results

### Simulator (Perfect Clock)
```
Expected A4: 440.0 Hz → Measured: 440.0 Hz  
Expected A5: 880.0 Hz → Measured: 880.0 Hz
Frequency Multiplier: 1.000000 (perfect match)
```

### Real Hardware (Fast Clock)  
```
Expected A4: 440.0 Hz → Measured: 441.3 Hz
Expected A5: 880.0 Hz → Measured: 882.6 Hz  
Frequency Multiplier: 1.003068 (hardware 0.3% fast)
```

### Real Hardware (Slow Clock)
```
Expected A4: 440.0 Hz → Measured: 438.1 Hz
Expected A5: 880.0 Hz → Measured: 876.2 Hz
Frequency Multiplier: 0.995682 (hardware 0.4% slow)  
```

## C Library Helper Functions

The library provides helper functions for RX implementation:

```c
// Get expected calibration frequencies
uint16_t expected_a4 = penta_get_calibration_frequency(PENTA_CALIBRATION_TONE_A4, use_enhanced);
uint16_t expected_a5 = penta_get_calibration_frequency(PENTA_CALIBRATION_TONE_A5, use_enhanced);

// Detect calibration sequence in received tone buffer
uint8_t tones[50];
// ... fill with detected tones ...
int is_calibration = penta_detect_calibration_sequence(tones, 50);

// Calculate multiplier from measurements
float multiplier = penta_calculate_frequency_multiplier(441.3f, 882.6f, use_enhanced);
```

## Benefits

### ✅ **Zero Manual Configuration**
- No need to measure hardware clock rates
- Works automatically for any clock variation
- Same RX code works for simulator and real hardware

### ✅ **High Accuracy**  
- Uses two reference frequencies for validation
- Perfect octave relationship provides error detection
- Extended tone duration enables precise frequency measurement

### ✅ **Robust Error Detection**
- Validates consistency between A4 and A5 measurements  
- Detects measurement errors automatically
- Fails gracefully if calibration is corrupted

### ✅ **Backward Compatible**
- Calibration sequence uses existing tone encoding
- No changes to block format or error correction
- Optional - RX can ignore calibration if not needed

## Integration Notes

### Transmission Flow
```
1. User calls penta_init_encoder() 
2. First call to penta_get_next_tone() returns calibration sequence (23 tones)
3. Subsequent calls return normal musical framing + data blocks
4. RX processes calibration first, then switches to normal decoding
```

### Performance Impact
- Adds 23 tones (~0.5-1.5 seconds) to transmission start
- Minimal impact on total transmission time  
- Greatly improves reliability across hardware variations

This automatic calibration system ensures that pentatonic audio transmissions work reliably across simulator and real hardware without any manual configuration or measurement.