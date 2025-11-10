# IrDA Upload Face - Simulator Support

This document explains how to test the IrDA upload face in the Emscripten simulator.

## Overview

The simulator includes built-in support for testing the IrDA upload face without physical hardware. The UI provides an easy way to upload files and send raw UART data.

**Important:** IrDA support is only available for `sensorwatch_pro` builds, as only that board has the IR sensor hardware.

## Building the Simulator

To build the simulator with IrDA support:

```bash
emmake make BOARD=sensorwatch_pro DISPLAY=classic
```

Serve the simulator locally:

```bash
cd build-sim
python -m http.server 8000
```

Then open `http://localhost:8000/firmware.html` in your browser.

## Using the Simulator

### Method 1: Upload Files via UI (Easiest)

The simulator includes a dedicated "IrDA/UART Upload Simulator" section below the shell console:

1. Navigate to the **IrDA Upload** face on the watch
2. Click **"Choose File"** and select a file to upload
   - Filename must be **12 characters or less**
   - File size should fit in the watch's **8KB filesystem**
3. Click **"Upload File"**
4. The file will be automatically formatted as an IrDA packet and injected into the UART buffer
5. The IrDA upload face should detect and process the file

**Example files to test:**
- Small text files (e.g., `hello.txt`, `test.json`)
- Binary files under a few KB
- Empty files (to test the upload flow)

### Method 2: Send Raw UART Data

You can also send raw data directly through the UI:

1. Enter data in the **text input field**:
   - **Hex format**: `48 65 6c 6c 6f` or `48656c6c6f` (will be converted to bytes)
   - **Text format**: `Hello` (will be sent as-is)
2. Click **"Send Data"**

This is useful for:
- Testing partial packets
- Sending malformed data to test error handling
- Debugging the IrDA protocol

### Method 3: JavaScript Console (Advanced)

For advanced testing, you can inject data from the browser's JavaScript console:

```javascript
// Inject raw bytes by setting the uart_rx_data variable
uart_rx_data = "\x48\x65\x6c\x6c\x6f";  // "Hello"

// Create and upload a properly formatted IrDA packet
function uploadFile(filename, content) {
    const data = new TextEncoder().encode(content);
    const packet = createIrdaPacket(filename, data);

    let dataStr = '';
    for (let i = 0; i < packet.length; i++) {
        dataStr += String.fromCharCode(packet[i]);
    }

    uart_rx_data = dataStr;
    console.log('Uploaded:', filename, '(', data.length, 'bytes)');
}

// Upload a test file
uploadFile("hello.txt", "Hello, Sensor Watch!");
```

The `createIrdaPacket()` function is already defined in the simulator's JavaScript.

## IrDA Packet Format

The UI automatically formats files into IrDA packets. The packet format is:

```
┌─────────────────────────────────────────────────────────┐
│ Header (16 bytes)                                       │
├──────────┬──────────────┬────────────────────────────┤
│ Size     │ Filename     │ Header Checksum             │
│ (2)      │ (12)         │ (2)                         │
├──────────┴──────────────┴────────────────────────────┤
│ Data (variable, if size > 0)                           │
├─────────────────────────────────────────────────────────┤
│ Data Checksum (2 bytes)                                │
└─────────────────────────────────────────────────────────┘
```

**Details:**
- **Size**: 2 bytes, little-endian uint16 (data length)
- **Filename**: 12 bytes, null-padded ASCII
- **Header Checksum**: 2 bytes, little-endian uint16 (sum of size + filename bytes)
- **Data**: Variable length bytes (if size > 0)
- **Data Checksum**: 2 bytes, little-endian uint16 (sum of all data bytes)

**Deletion**: To delete a file, send a packet with size=0 and the filename to delete.

## Implementation Details

### Architecture

The simulator follows the same architecture pattern as other watch peripherals (buttons, shell, etc.):

```
┌──────────────────┐
│   HTML/UI        │  File input, buttons
│   (shell.html)   │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│   JavaScript     │  createIrdaPacket(), uploadViaUart()
│   (shell.html)   │  Sets global: uart_rx_data = "..."
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│   C Code         │  uart_sim_poll_js_data()
│   (uart.c)       │  Reads: EM_ASM({ return uart_rx_data; })
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│   Watch Face     │  uart_read_instance()
│ (irda_upload_    │  Processes IrDA packets
│  face.c)         │
└──────────────────┘
```

### Key Files

- **C Implementation**: `watch-library/simulator/peripherals/uart.c`
  - Ring buffer (512 bytes)
  - Polls JavaScript `uart_rx_data` variable using `EM_ASM`
  - Implements all UART functions including IrDA mode

- **JavaScript**: `watch-library/simulator/shell.html`
  - Global variable: `uart_rx_data`
  - Functions: `createIrdaPacket()`, `uploadViaUart()`, `sendUartData()`
  - UI elements for file upload and raw data input

- **Pattern**: Same approach as shell console (which uses `tx` variable)
  - No Emscripten function exports required
  - C code pulls data from JavaScript using `EM_ASM`
  - Clean separation following existing simulator architecture

### Makefile Integration

The simulator build automatically:
1. Filters out gossamer's dummy UART implementation
2. Includes custom UART from `watch-library/simulator/peripherals/uart.c`
3. No special Emscripten exports needed (uses existing runtime methods)

## Limitations

1. **Board-specific**: Only works with sensorwatch_pro simulator builds
2. **No visual IR LED**: No visual indicator when receiving data (use shell output)
3. **Instant injection**: No actual baud rate timing (900 baud IrDA is instant)
4. **Buffer size**: 512 byte ring buffer (large files work fine, buffered automatically)

## Troubleshooting

**IrDA upload face not appearing:**
- Ensure you built with `BOARD=sensorwatch_pro`
- Check that `HAS_IR_SENSOR` is defined in build log
- Verify the IrDA face is enabled in `movement_config.h`

**File upload not working:**
- Ensure the watch is on the **IrDA Upload face** (not shell or another face)
- Check browser console for JavaScript errors
- Verify filename is 12 characters or less
- Try a smaller file first (< 1KB)

**Filename too long error:**
- Rename your file to 12 characters or less (e.g., `mylongfilename.txt` → `myfile.txt`)

**Shell commands vs UART:**
- Shell commands (like `ls`, `cat`) use the `tx` variable
- UART/IrDA data uses the `uart_rx_data` variable
- They are independent systems

## Testing Checklist

Use this checklist to verify simulator functionality:

- [ ] Build simulator with `emmake make BOARD=sensorwatch_pro`
- [ ] Open simulator in browser
- [ ] Navigate to IrDA Upload face
- [ ] Upload a small text file (< 100 bytes)
- [ ] Verify file appears in filesystem (use shell `ls` command)
- [ ] Read file back (use shell `cat filename`)
- [ ] Upload a binary file
- [ ] Test filename length limit (try 12 char filename)
- [ ] Test hex data input
- [ ] Test raw text input
- [ ] Check browser console for any errors

## Python Script

For testing the IrDA protocol outside the simulator, use `scripts/irda_transmitter.py`:

```bash
# Upload a file via physical IrDA
python3 scripts/irda_transmitter.py /dev/ttyUSB0 test.txt myfile.txt

# Delete a file
python3 scripts/irda_transmitter.py /dev/ttyUSB0 test.txt --delete
```

This script creates the exact same packet format as the simulator's JavaScript code.
