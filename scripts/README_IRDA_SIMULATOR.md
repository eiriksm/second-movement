# IrDA Upload Face - Simulator Support

This document explains how to test the IrDA upload face in the Emscripten simulator.

## Overview

The simulator now includes support for the IrDA upload face, which allows you to test file upload functionality without physical hardware.

**Important:** IrDA support is only available for `sensorwatch_pro` builds, as only that board has the IR sensor hardware.

## Building the Simulator

To build the simulator with IrDA support:

```bash
emmake make BOARD=sensorwatch_pro DISPLAY=classic
```

The `HAS_IR_SENSOR` flag is automatically defined for sensorwatch_pro via its board configuration. The custom UART implementation is automatically included for all simulator builds.

## Testing in the Simulator

### Method 1: Direct Filesystem API (Easiest)

The simplest way to upload files is using the exported filesystem functions directly:

```javascript
// Helper to write a file to the watch filesystem
function writeFile(filename, content) {
    // Allocate filename string
    const filenameLen = Module.lengthBytesUTF8(filename) + 1;
    const filenamePtr = Module._malloc(filenameLen);
    Module.stringToUTF8(filename, filenamePtr, filenameLen);

    // Allocate content buffer
    const contentBytes = new TextEncoder().encode(content);
    const contentPtr = Module._malloc(contentBytes.length);
    Module.HEAPU8.set(contentBytes, contentPtr);

    // Call filesystem_write_file
    const result = Module._filesystem_write_file(filenamePtr, contentPtr, contentBytes.length);

    // Cleanup
    Module._free(filenamePtr);
    Module._free(contentPtr);

    console.log(result ? "✓ File written successfully" : "✗ File write failed");
    return result !== 0;
}

// Example usage
writeFile("test.txt", "Hello from JavaScript!");

// Check free space
console.log("Free space:", Module._filesystem_get_free_space(), "bytes");
```

### Method 2: IrDA Protocol Simulation (Testing IR Face)

If you want to test the actual IrDA upload face, you can inject IrDA data via UART:

```javascript
// Helper function to inject raw bytes into UART buffer
function injectIrDAData(data) {
    // Convert data array to Uint8Array if needed
    const uint8Array = data instanceof Uint8Array ? data : new Uint8Array(data);

    // Allocate memory in Emscripten heap
    const dataPtr = Module._malloc(uint8Array.length);
    Module.HEAPU8.set(uint8Array, dataPtr);

    // Call the inject function (sercom 0 is used by IrDA face)
    // Using direct function call (more reliable):
    Module._uart_sim_inject_data(0, dataPtr, uint8Array.length);

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

### Method 3: Python Script

Use the included `irda_transmitter.py` script to generate test packets:

```bash
# Generate a test file
echo "Hello Watch" > /tmp/testfile.txt

# Create packet (this outputs the bytes, you'll need to manually inject)
python3 scripts/irda_transmitter.py /dev/null test.txt /tmp/testfile.txt
```

Then copy the packet bytes and inject them using Method 1 above.

### Method 4: Automated Testing (Future Enhancement)

A more sophisticated approach would be to:

1. Add Emscripten's `EM_ASM` or `EXPORTED_FUNCTIONS` to expose `uart_sim_inject_data`
2. Create a file upload UI button in the simulator HTML
3. Use FileReader API to read files and inject them automatically

This would require modifying the `shell.html` template.

## Implementation Details

### UART Simulator

The custom UART implementation (`watch-library/simulator/peripherals/uart.c`) includes:

- **Ring buffer** for received data (512 bytes)
- **`uart_sim_inject_data(sercom, data, length)`** - Inject data into UART buffer
- **`uart_sim_get_buffer_count(sercom)`** - Check buffer status
- **Full IrDA mode support** via `uart_set_irda_mode_instance()`

### Enabled Features

When building the sensorwatch_pro simulator:

- `HAS_IR_SENSOR` define (from sensorwatch_pro board configuration)
- IrDA upload face in simulator build
- UART buffer implementation for SERCOM 0 (via custom simulator peripheral)

## Limitations

1. **Board-specific** - Only works with sensorwatch_pro simulator builds
2. **No actual IR hardware** - Data must be injected manually via JavaScript
3. **Buffer size** - Limited to 512 bytes per injection
4. **No visual feedback** - Watch face shows upload status on simulated LCD only
5. **Timing** - No actual baud rate emulation (instant injection)

## Future Improvements

Possible enhancements:

- WebSocket server to receive packets from external tools
- Drag-and-drop file upload in simulator UI
- Proper baud rate timing simulation
- Visual IR LED indicator in simulator

## Troubleshooting

**Face not appearing:**
- Ensure you built with `BOARD=sensorwatch_pro` (other boards don't have IR sensor)
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
