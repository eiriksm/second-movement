# FESK TX Unit Tests

Unit tests for the FESK (Frequency Shift Keying) audio transmission library.

## Running Tests

```bash
cd lib/fesk_tx/test
make test
```

Or simply:

```bash
make clean && make test
```

## Test Framework

Tests use [Unity](https://github.com/ThrowTheSwitch/Unity), a lightweight C unit testing framework (MIT licensed).

## Test Coverage

### Basic Functionality
- **test_encode_simple_text** - Basic encoding operation
- **test_encode_case_insensitive** - Case-insensitive character handling
- **test_encode_all_characters** - Complete character set support

### Character Support
- **test_encode_digits** - Numeric characters (0-9)
- **test_encode_punctuation** - Supported punctuation (, : ' ")
- **test_encode_newline** - Newline character encoding

### Error Handling
- **test_encode_unsupported_character** - Rejection of unsupported characters
- **test_encode_null_pointer** - NULL pointer argument handling
- **test_encode_empty_string** - Empty input rejection

### Edge Cases
- **test_encode_max_length** - Maximum message length (1024 chars)
- **test_encode_over_max_length** - Over-length message rejection
- **test_free_null_sequence** - NULL-safe cleanup function

### Correctness Validation
- **test_sequence_structure** - Sequence format verification
- **test_tone_mapping** - FSK tone assignments
- **test_different_inputs_different_sequences** - Output uniqueness

## What's Tested

These tests cover **fesk_tx.c** (core encoding logic):
- ✅ All public API functions (`fesk_encode_cstr`, `fesk_encode_text`, `fesk_free_sequence`)
- ✅ All supported characters (a-z, A-Z, 0-9, space, punctuation)
- ✅ All error conditions and return codes
- ✅ Boundary conditions and edge cases
- ✅ Output sequence structure and format

## What's Not Tested

**fesk_session.c** (session management) is not tested here due to hardware dependencies. Those functions require:
- Hardware timer mocks
- Buzzer playback simulation
- Display indicator mocks
- Asynchronous callback handling

Testing the session layer would require a more complex integration test setup.

## Expected Output

All tests should pass:

```
test_main.c:262:test_encode_simple_text:PASS
test_main.c:263:test_encode_case_insensitive:PASS
...
-----------------------
15 Tests 0 Failures 0 Ignored
OK
```

## Requirements

- GCC or compatible C compiler
- Standard C library (stdlib, string.h)
- Make

## Test File Structure

- `test_main.c` - Test cases
- `unity.c` / `unity.h` / `unity_internals.h` - Unity test framework
- `watch_tcc.h` - Minimal mock for watch hardware definitions
- `Makefile` - Build configuration
- `.gitignore` - Build artifact exclusions
