# Poker Tournament Timer for CYD

A poker tournament blind timer for the ESP32-2432S028 "Cheap Yellow Display" (CYD) with touchscreen interface.

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Display](https://img.shields.io/badge/display-ILI9341-green)
![Touch](https://img.shields.io/badge/touch-XPT2046-orange)

## Features

- 🎰 **25 Configurable Tournament Rounds** - Customize blinds, antes, and break periods
- ⏱️ **Auto-Advancing Timer** - Automatically progresses through rounds
- 📱 **Touchscreen Controls** - Start, pause, skip forward/back
- 🔊 **Audio Alerts** - Buzzer beeps on button press and 5-second alert at round end
- 🌐 **WiFi Configuration** - Web-based setup interface
- 💾 **Persistent Storage** - Settings and timer state saved to flash
- 🔄 **Auto-Resume** - Continues from last position after power loss
- ⚙️ **Easy Config Mode** - Tap gear icon to configure

## Hardware Requirements

- **ESP32-2432S028** (CYD - Cheap Yellow Display)
  - 240x320 ILI9341 TFT Display
  - XPT2046 Resistive Touch Controller
  - ESP32 with WiFi
- **Piezo Buzzer** (optional) - Connected to P4 speaker connector (GPIO 26)

## Display Layout

```
┌────────────────────┐
│ Round 1      [⚙️]  │  ← Round info & config gear
├────────────────────┤
│ Blinds: 25/50      │  ← Current blinds
│ Ante: 0            │  ← Ante (if any)
│                    │
│      15:00         │  ← Large countdown timer
│                    │
│     RUNNING        │  ← Timer status
├────────────────────┤
│ [START][NEXT][PREV]│  ← Touch buttons
└────────────────────┘
```

## Quick Start

### Option 1: Web Flasher (Easiest!)

**No software installation required!** Flash directly from your browser:

1. Visit **[https://rfltools.github.io/Poker_Timer_Lite_CYD/](https://rfltools.github.io/Poker_Timer_Lite_CYD/)**
2. Connect your ESP32-2432S028 via USB
3. Click "INSTALL POKER TIMER"
4. Select your device's COM port
5. Wait for installation to complete (~1-2 minutes)

**Requirements:**
- Chrome, Edge, or Opera browser (Web Serial API support)
- USB cable to connect device
- CP210x or CH340 USB driver (usually auto-installed)

### Option 2: PlatformIO (Advanced)

```bash
# In PlatformIO
pio run --target upload
```

### 2. First Run

The timer starts with default settings:
- 25 rounds (15 minutes each)
- Breaks every 4th round
- Blinds starting at 25/50, doubling progression

### 3. Configure Tournament (Optional)

**To enter configuration mode:**

1. Tap the **⚙️ gear icon** (top-right corner)
2. Confirm by tapping **YES**
3. Connect to WiFi network: **PokerTimer**
4. Open browser: **http://192.168.4.1**
5. Configure your tournament settings
6. Click **Save** - device reboots

### 4. Using the Timer

**Touch Controls:**
- **START** (green) - Start the timer
- **PAUSE** (red) - Pause the timer
- **NEXT** (blue) - Skip to next round
- **PREV** (orange) - Go to previous round
- **⚙️ Gear** - Enter configuration mode

**Auto Features:**
- Timer automatically advances when round finishes
- Settings auto-save every 10 seconds
- Timer state preserved on power loss

## Configuration

### platformio.ini

The project uses these critical settings for CYD:

```ini
[env:esp32-2432s028]
platform = espressif32
board = esp32dev
framework = arduino

build_flags = 
    -D ILI9341_2_DRIVER=1          # CYD display driver
    -D TFT_INVERSION_ON=1          # Color correction
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    # Display pins
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=21
    # Touch pins (separate SPI bus!)
    -D TOUCH_CS=33
    -D TOUCH_MOSI=32
    -D TOUCH_MISO=39
    -D TOUCH_CLK=25
    -D TOUCH_IRQ=36
```

### Libraries Used

- `TFT_eSPI` - Display driver
- `XPT2046_Touchscreen` - Touch controller
- `ESPAsyncWebServer` - Configuration web interface
- `AsyncTCP` - Async networking
- `ArduinoJson` - JSON parsing
- `Preferences` - NVS storage

## Default Tournament Structure

**Rounds 1-3, 5-7, 9-11, etc.:** Play rounds with blinds
- Round 1: 25/50
- Round 2: 50/100
- Round 3: 75/150
- Continues doubling...

**Rounds 4, 8, 12, etc.:** 15-minute breaks

All configurable via web interface!

## Technical Details

### Display Configuration

- **Driver:** ILI9341_2_DRIVER (variant 2 for CYD)
- **Rotation:** 0 (portrait mode, 240x320)
- **Inversion:** ON (color correction)
- **SPI Bus:** VSPI (default ESP32 SPI)

### Touch Configuration

- **Controller:** XPT2046 resistive touch
- **SPI Bus:** HSPI (separate from display!)
- **Rotation:** 0 (matches display)
- **Calibration:** Pre-configured for standard CYD

**Important:** CYD uses **two separate SPI buses** - one for display (VSPI) and one for touch (HSPI). This is critical for proper operation.

### Pin Configuration

**Display (VSPI):**
- MOSI: GPIO 13
- MISO: GPIO 12
- SCLK: GPIO 14
- CS: GPIO 15
- DC: GPIO 2
- BL: GPIO 21

**Touch (HSPI):**
- MOSI: GPIO 32
- MISO: GPIO 39
- CLK: GPIO 25
- CS: GPIO 33
- IRQ: GPIO 36

## Troubleshooting

### Touch Not Responding

- Touch requires firm pressure (resistive touchscreen)
- Watch Serial Monitor for touch coordinates
- Default calibration works for most CYD units

### Display Issues

- If colors inverted: Toggle `TFT_INVERSION_ON/OFF`
- If display sideways: Change `tft.setRotation(0)` to 1, 2, or 3
- If text garbled: Try `ILI9341_DRIVER` instead of `ILI9341_2_DRIVER`

### Config Mode Not Working

- Gear icon is in **top-right corner**
- Requires YES confirmation to prevent accidental entry
- 10-second timeout if no response

### Serial Monitor

Connect at **115200 baud** to see:
- Initialization status
- Touch coordinates (for debugging)
- Round changes
- Error messages

## Web Flasher Development

Want to build and host the web flasher yourself?

### Building Locally

```bash
# 1. Build firmware with PlatformIO
pio run --environment esp32-2432s028

# 2. Copy binaries to web_flasher directory
# Windows:
.\build_web_flasher.ps1

# Linux/Mac:
chmod +x build_web_flasher.sh
./build_web_flasher.sh

# 3. Test locally
cd web_flasher
python -m http.server 8000
# Open http://localhost:8000 in Chrome
```

### Automatic GitHub Pages Deployment

The project includes a GitHub Actions workflow that:
1. Automatically builds firmware on every push to main
2. Deploys the web flasher to GitHub Pages
3. Makes it available at: `https://yourusername.github.io/yourrepo/`

See `.github/workflows/build.yml` and `web_flasher/README.md` for details.

## Project Structure

```
Poker_Timer_CYD/
├── src/
│   └── main.cpp              # Main application code
├── web_flasher/              # Web-based firmware flasher
│   ├── index.html            # Flasher web interface
│   ├── manifest.json         # ESP Web Tools manifest
│   └── README.md             # Web flasher documentation
├── .github/
│   └── workflows/
│       └── build.yml         # CI/CD for web flasher
├── platformio.ini            # Build configuration
├── no_ota.csv               # Partition table
├── build_web_flasher.ps1    # Windows build script
├── build_web_flasher.sh     # Linux/Mac build script
├── README.md                # This file
├── README_CYD.md            # Detailed CYD information
├── CONFIG_MODE_NEW.md       # Config mode documentation
└── QUICK_START.md           # Quick start guide
```

## Contributing

This project was developed specifically for the CYD (ESP32-2432S028) hardware. The configuration has been tested and verified working on standard CYD units.

## License

Open source - free to use for personal and commercial purposes.

## Acknowledgments

- Built with PlatformIO
- Uses TFT_eSPI library by Bodmer
- XPT2046 touch library by Paul Stoffregen
- Async web server by ESPHome

---

**Enjoy your poker tournaments!** 🎰🃏
