# FESK TX

Frequency Shift Keying (FSK) audio transmission library for Sensor Watch.

## Protocol

FESK encodes text messages into dual-tone audio sequences transmitted via the piezo buzzer:

- **Binary '0'**: D7# (~2489 Hz)
- **Binary '1'**: G7 (~3136 Hz)
- **Timing**: 1 tick tone + 2 ticks silence per bit (~47ms @ 64Hz)

### Frame Format

```
[START(6bit)] [PAYLOAD(N×6bit)] [CRC8(8bit)] [END(6bit)]
```

- **START marker**: Code 62 (binary `111110`)
- **Payload**: 6-bit character codes (0-41)
- **CRC-8**: Polynomial 0x07 for error detection
- **END marker**: Code 63 (binary `111111`)

### Character Set

42 supported characters (case-insensitive):
- Letters: `a-z` (codes 0-25)
- Digits: `0-9` (codes 26-35)
- Punctuation: space `,` `:` `'` `"` `\n` (codes 36-41)

### Technical Details

- **Max message length**: 1024 bytes
- **Bit rate**: ~21.3 bits/second
- **Error detection**: CRC-8 catches all single-bit errors and most burst errors

## Usage

See `fesk_tx.h` and `fesk_session.h` for API documentation.
