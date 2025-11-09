#!/bin/bash
# ARM Toolchain Setup Script for Second Movement
# This script downloads and installs the correct version of the ARM toolchain (10.3)

set -e

TOOLCHAIN_DIR="$HOME/.local/arm-toolchain"
TOOLCHAIN_VERSION="10.3-2021.07"
TOOLCHAIN_NAME="gcc-arm-none-eabi-${TOOLCHAIN_VERSION}"

echo "Setting up ARM GCC toolchain ${TOOLCHAIN_VERSION}..."

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    TOOLCHAIN_FILE="${TOOLCHAIN_NAME}-x86_64-linux.tar.bz2"
elif [ "$ARCH" = "aarch64" ]; then
    TOOLCHAIN_FILE="${TOOLCHAIN_NAME}-aarch64-linux.tar.bz2"
else
    echo "Error: Unsupported architecture: $ARCH"
    exit 1
fi

# Check if toolchain is already installed
if [ -d "$TOOLCHAIN_DIR" ] && [ -f "$TOOLCHAIN_DIR/bin/arm-none-eabi-gcc" ]; then
    INSTALLED_VERSION=$("$TOOLCHAIN_DIR/bin/arm-none-eabi-gcc" --version | head -n1)
    echo "Toolchain already installed: $INSTALLED_VERSION"
    echo "Toolchain location: $TOOLCHAIN_DIR"
    echo ""
    echo "Add to your PATH with:"
    echo "  export PATH=\"$TOOLCHAIN_DIR/bin:\$PATH\""
    exit 0
fi

# Create toolchain directory
mkdir -p "$TOOLCHAIN_DIR"

echo "Downloading ARM GCC toolchain..."
echo "Note: If the download fails, you may need to:"
echo "  1. Download manually from: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads/"
echo "  2. Or use apt: sudo apt install gcc-arm-none-eabi (version may differ)"
echo "  3. Or use the devcontainer: See .devcontainer/README.md"
echo ""

# Try to download from xpack GitHub releases (more reliable)
XPACK_URL="https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v10.3.1-2.3/xpack-arm-none-eabi-gcc-10.3.1-2.3-linux-x64.tar.gz"

if command -v wget > /dev/null; then
    wget -O /tmp/toolchain.tar.gz "$XPACK_URL" || {
        echo "Error: Download failed. Please install manually."
        exit 1
    }
elif command -v curl > /dev/null; then
    curl -L -o /tmp/toolchain.tar.gz "$XPACK_URL" || {
        echo "Error: Download failed. Please install manually."
        exit 1
    }
else
    echo "Error: Neither wget nor curl found. Please install one of them."
    exit 1
fi

echo "Extracting toolchain..."
tar -xzf /tmp/toolchain.tar.gz -C /tmp/
mv /tmp/xpack-arm-none-eabi-gcc-*/* "$TOOLCHAIN_DIR/"
rm -rf /tmp/toolchain.tar.gz /tmp/xpack-arm-none-eabi-gcc-*

echo ""
echo "Toolchain installed successfully!"
echo "Location: $TOOLCHAIN_DIR"
echo ""
echo "To use this toolchain, add it to your PATH:"
echo "  export PATH=\"$TOOLCHAIN_DIR/bin:\$PATH\""
echo ""
echo "Or add this line to your ~/.bashrc or ~/.zshrc:"
echo "  export PATH=\"$TOOLCHAIN_DIR/bin:\$PATH\""
