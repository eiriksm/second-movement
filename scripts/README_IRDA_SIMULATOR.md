# IrDA Upload Face - Simulator Support

This document explains how to test the IrDA upload face in the Emscripten simulator.

## Overview

The simulator now includes support for the IrDA upload face, which allows you to test file upload functionality without physical hardware.

**Important:** IrDA support is only available for `sensorwatch_pro` builds, as only that board has the IR sensor hardware.

## Building the Simulator

To build the simulator with IrDA support, you must:

1. **Apply the gossamer UART patch** (required for simulator UART support):
```bash
cd gossamer
git apply ../scripts/gossamer_uart_simulator.patch
cd ..
```

2. **Build the sensorwatch_pro simulator**:
```bash
emmake make BOARD=sensorwatch_pro DISPLAY=classic
```

The `HAS_IR_SENSOR` flag is automatically defined for sensorwatch_pro via its board configuration.

## Testing in the Simulator

### Method 1: JavaScript Console (Simple Testing)

Once the simulator is running in your browser, you can inject IrDA data using the browser's JavaScript console:

```javascript
// Helper function to inject raw bytes into UART buffer
function injectIrDAData(data) {
    // Convert data array to Uint8Array
    const uint8Array = new Uint8Array(data);

    // Allocate memory in Emscripten heap
    const dataPtr = Module._malloc(uint8Array.length);
    Module.HEAPU8.set(uint8Array, dataPtr);

    // Call the inject function (sercom 0 is used by IrDA face)
    Module.ccall('uart_sim_inject_data', null, ['number', 'number', 'number'],
                 [0, dataPtr, uint8Array.length]);

    // Free allocated memory
    Module._free(dataPtr);
}

// Example: Upload a small file named "test.txt" with content "Hello"
function uploadTestFile() {
    // IrDA packet format (from irda_upload_face.c:76-79):
    // Header: [Size(2)][Filename(12)][Header_Checksum(2)]
    // Data:   [Data(size bytes)][Data_Checksum(2)]

    const filename = "test.txt";
    const content = "Hello";

    // Create header
    const size = content.length;
    const filenameBytes = new Array(12).fill(0);
    for (let i = 0; i < Math.min(filename.length, 12); i++) {
        filenameBytes[i] = filename.charCodeAt(i);
    }

    const header = [
        size & 0xFF, (size >> 8) & 0xFF,  // Size (2 bytes, little-endian)
        ...filenameBytes                    // Filename (12 bytes)
    ];

    // Calculate header checksum
    const headerChecksum = header.reduce((sum, byte) => (sum + byte) & 0xFFFF, 0);
    header.push(headerChecksum & 0xFF, (headerChecksum >> 8) & 0xFF);

    // Add content
    const data = [...header];
    for (let i = 0; i < content.length; i++) {
        data.push(content.charCodeAt(i));
    }

    // Calculate data checksum (only the content bytes, not header)
    const contentBytes = [];
    for (let i = 0; i < content.length; i++) {
        contentBytes.push(content.charCodeAt(i));
    }
    const dataChecksum = contentBytes.reduce((sum, byte) => (sum + byte) & 0xFFFF, 0);
    data.push(dataChecksum & 0xFF, (dataChecksum >> 8) & 0xFF);

    // Inject the packet
    injectIrDAData(data);
    console.log("Uploaded file:", filename, "with content:", content);
}

// Run the test
uploadTestFile();
```

### Method 2: Python Script

Use the included `irda_transmitter.py` script to generate test packets:

```bash
# Generate a test file
echo "Hello Watch" > /tmp/testfile.txt

# Create packet (this outputs the bytes, you'll need to manually inject)
python3 scripts/irda_transmitter.py /dev/null test.txt /tmp/testfile.txt
```

Then copy the packet bytes and inject them using Method 1 above.

### Method 3: Automated Testing (Future Enhancement)

A more sophisticated approach would be to:

1. Add Emscripten's `EM_ASM` or `EXPORTED_FUNCTIONS` to expose `uart_sim_inject_data`
2. Create a file upload UI button in the simulator HTML
3. Use FileReader API to read files and inject them automatically

This would require modifying the `shell.html` template.

## Implementation Details

### UART Simulator

The dummy UART implementation (`gossamer/dummy/peripherals/uart.c`) now includes:

- **Ring buffer** for received data (512 bytes)
- **`uart_sim_inject_data(sercom, data, length)`** - Inject data into UART buffer
- **`uart_sim_get_buffer_count(sercom)`** - Check buffer status
- **Full IrDA mode support** via `uart_set_irda_mode_instance()`

### Enabled Features

When building the sensorwatch_pro simulator (with the UART patch applied):

- `HAS_IR_SENSOR` define (from sensorwatch_pro board configuration)
- IrDA upload face in simulator build
- UART buffer implementation for SERCOM 0 (via patched gossamer dummy peripheral)

## Limitations

1. **Board-specific** - Only works with sensorwatch_pro simulator builds
2. **No actual IR hardware** - Data must be injected manually via JavaScript
3. **Requires patch** - Must apply gossamer UART patch before building
4. **Buffer size** - Limited to 512 bytes per injection
5. **No visual feedback** - Watch face shows upload status on simulated LCD only
6. **Timing** - No actual baud rate emulation (instant injection)

## Future Improvements

Possible enhancements:

- WebSocket server to receive packets from external tools
- Drag-and-drop file upload in simulator UI
- Proper baud rate timing simulation
- Visual IR LED indicator in simulator

## Troubleshooting

**Face not appearing:**
- Ensure you built with `BOARD=sensorwatch_pro` (other boards don't have IR sensor)
- Verify you applied the gossamer UART patch before building
- Check that `HAS_IR_SENSOR` is defined in build log
- Verify IrDA face is in `movement_faces.h`

**Data not received:**
- Check that watch is on the IrDA upload face
- Verify packet format matches specification
- Check browser console for errors
- Ensure checksums are correct

**Buffer overflow:**
- Packets larger than 512 bytes need multiple injections
- Add delays between injections if needed
