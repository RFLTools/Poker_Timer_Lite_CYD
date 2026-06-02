#!/bin/bash
# Shell script to build and prepare web flasher files
# Run this after building the firmware with PlatformIO

echo "================================================"
echo "  Poker Timer CYD - Web Flasher Builder"
echo "================================================"
echo ""

# Check if build directory exists
if [ ! -d ".pio/build/esp32-2432s028" ]; then
    echo "Error: Build directory not found!"
    echo "Please build the firmware first using PlatformIO:"
    echo "  pio run --environment esp32-2432s028"
    echo "Or use the PlatformIO Build button in VS Code"
    exit 1
fi

# Create web_flasher directory if it doesn't exist
if [ ! -d "web_flasher" ]; then
    echo "Creating web_flasher directory..."
    mkdir -p web_flasher
fi

echo "Copying firmware binaries..."

# Function to copy and show size
copy_with_size() {
    local src=$1
    local dest=$2
    local name=$(basename "$dest")
    
    if [ -f "$src" ]; then
        cp "$src" "$dest"
        local size=$(du -h "$dest" | cut -f1)
        echo "  ✓ $name ($size)"
    else
        echo "  ✗ $name not found!"
    fi
}

# Copy firmware binary
copy_with_size ".pio/build/esp32-2432s028/firmware.bin" "web_flasher/firmware.bin"

# Copy bootloader
copy_with_size ".pio/build/esp32-2432s028/bootloader.bin" "web_flasher/bootloader.bin"

# Copy partitions
copy_with_size ".pio/build/esp32-2432s028/partitions.bin" "web_flasher/partitions.bin"

# Look for boot_app0.bin in common locations
BOOT_APP_LOCATIONS=(
    "$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    ".pio/build/esp32-2432s028/boot_app0.bin"
)

BOOT_APP_FOUND=false
for location in "${BOOT_APP_LOCATIONS[@]}"; do
    if [ -f "$location" ]; then
        cp "$location" "web_flasher/boot_app0.bin"
        BOOT_APP_FOUND=true
        size=$(du -h "web_flasher/boot_app0.bin" | cut -f1)
        echo "  ✓ boot_app0.bin ($size)"
        break
    fi
done

if [ "$BOOT_APP_FOUND" = false ]; then
    echo "  ! boot_app0.bin not found, creating..."
    # Create a blank boot_app0.bin (4KB filled with 0xFF)
    python3 -c "import os; f=open('web_flasher/boot_app0.bin','wb'); f.write(b'\\xff'*4096); f.close()"
    echo "  ✓ boot_app0.bin created (4.0K)"
fi

echo ""
echo "================================================"
echo "Build complete! Files ready in web_flasher/"
echo "================================================"
echo ""
echo "To test locally:"
echo "  1. cd web_flasher"
echo "  2. python3 -m http.server 8000"
echo "  3. Open http://localhost:8000 in Chrome/Edge"
echo ""
echo "Files in web_flasher directory:"
ls -lh web_flasher/ | grep -v "^total" | grep -v "^d" | awk '{printf "  %-20s %10s\n", $9, $5}'
echo ""
