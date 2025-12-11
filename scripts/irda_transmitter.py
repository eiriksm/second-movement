#!/usr/bin/env python3
"""
IrDA File Transmitter for Second Movement Watch

Transmits files to the watch using IrDA protocol at 900 baud.
Requires an IrDA-capable device or IR transmitter connected via serial port.

Protocol format (from irda_upload_face.c:76-79):
  Header: [Size(2)][Filename(12)][Header_Checksum(2)]
  Data:   [Data(size bytes)][Data_Checksum(2)]

Usage:
  python3 irda_transmitter.py <serial_port> <filename> [data_file]

  To upload:  python3 irda_transmitter.py /dev/ttyUSB0 myfile.txt data.txt
  To delete:  python3 irda_transmitter.py /dev/ttyUSB0 myfile.txt
"""

import serial
import struct
import sys
import time
from pathlib import Path


def calculate_checksum(data: bytes) -> int:
    """Calculate simple sum checksum used by the watch."""
    return sum(data) & 0xFFFF


def create_packet(filename: str, data: bytes = b'') -> bytes:
    """
    Create IrDA packet for file upload or deletion.

    Args:
        filename: Name of file (max 12 chars, will be truncated/padded)
        data: File contents (empty for deletion)

    Returns:
        Complete packet bytes ready for transmission
    """
    # Prepare filename (max 12 bytes, null-padded)
    filename_bytes = filename.encode('ascii')[:12].ljust(12, b'\x00')

    # Get data size
    data_size = len(data)

    # Build header: Size(2) + Filename(12) + Checksum(2)
    header = struct.pack('<H', data_size) + filename_bytes
    header_checksum = calculate_checksum(header)
    header += struct.pack('<H', header_checksum)

    # If no data, we're done (deletion request)
    if data_size == 0:
        return header

    # Add data and data checksum
    data_checksum = calculate_checksum(data)
    packet = header + data + struct.pack('<H', data_checksum)

    return packet


def transmit_file(port: str, filename: str, data_file: str = None):
    """
    Transmit file to watch via IrDA.

    Args:
        port: Serial port device (e.g., '/dev/ttyUSB0' or 'COM3')
        filename: Name for file on watch (max 12 chars)
        data_file: Path to file to upload (None for deletion)
    """
    # Read data if provided
    if data_file:
        data = Path(data_file).read_bytes()
        print(f"Uploading '{filename}' ({len(data)} bytes) to watch...")
    else:
        data = b''
        print(f"Deleting '{filename}' from watch...")

    # Create packet
    packet = create_packet(filename, data)
    print(f"Packet size: {len(packet)} bytes")

    # Open serial port with IrDA settings (900 baud, 8N1)
    try:
        ser = serial.Serial(
            port=port,
            baudrate=900,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )

        print(f"Opened {port} at 900 baud")
        print("Transmitting...")

        # Send packet
        ser.write(packet)
        ser.flush()

        # Wait a moment for transmission
        time.sleep(len(packet) / 90 + 0.5)  # ~90 bytes/sec + buffer

        ser.close()
        print("Transmission complete!")

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    port = sys.argv[1]
    filename = sys.argv[2]
    data_file = sys.argv[3] if len(sys.argv) > 3 else None

    # Validate filename length
    if len(filename) > 12:
        print(f"Warning: Filename '{filename}' exceeds 12 chars, will be truncated")
        filename = filename[:12]

    transmit_file(port, filename, data_file)


if __name__ == '__main__':
    main()
