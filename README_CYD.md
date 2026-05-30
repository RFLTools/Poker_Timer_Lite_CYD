# CYD Hardware Information

Technical details specific to the ESP32-2432S028 "Cheap Yellow Display" (CYD).

## Hardware Specifications

### Display
- **Model:** ILI9341 TFT LCD
- **Resolution:** 240x320 pixels
- **Interface:** SPI (VSPI bus)
- **Driver Variant:** ILI9341_2_DRIVER
- **Color Depth:** 16-bit (RGB565)
- **Backlight:** GPIO 21, active HIGH

### Touch Controller
- **Model:** XPT2046
- **Type:** Resistive touch (pressure-sensitive)
- **Interface:** SPI (HSPI bus - **separate from display!**)
- **Resolution:** 12-bit (4096 levels)
- **IRQ:** GPIO 36

### ESP32 Module
- **Chip:** ESP32-WROOM-32
- **Flash:** 4MB
- **RAM:** 520KB
- **WiFi:** 802.11 b/g/n
- **Bluetooth:** BLE & Classic

### Buzzer/Speaker Output
- **Connector:** P4 (2-pin JST 1.25mm)
- **Control Pin:** GPIO 26 (DAC capable)
- **Amplifier:** SC8002B/LET7525AS onboard amplifier IC
- **Usage:** Connect piezo buzzer or small speaker (< 3W)

## Critical CYD Configuration

### Two Separate SPI Buses!

The CYD uses **two independent SPI buses**:

**VSPI (Display):**
```
MOSI: GPIO 13
MISO: GPIO 12
SCLK: GPIO 14
CS:   GPIO 15
DC:   GPIO 2
BL:   GPIO 21
```

**HSPI (Touch):**
```
MOSI: GPIO 32
MISO: GPIO 39
CLK:  GPIO 25
CS:   GPIO 33
IRQ:  GPIO 36
```

This separation is **critical** - the touch controller will not work if you try to use the display's SPI bus!

### Display Driver Settings

```ini
-D ILI9341_2_DRIVER=1      # Use variant 2 (not standard ILI9341_DRIVER)
-D TFT_INVERSION_ON=1      # Required for correct colors on CYD
```

**Why variant 2?** The CYD uses a specific ILI9341 chip variant that requires different initialization sequences.

**Why inversion ON?** The CYD's display wiring requires inverted color signals for correct black/white rendering.

### Rotation Configuration

```cpp
tft.setRotation(0);  // Portrait: 240 wide x 320 tall
ts.setRotation(0);   // Must match display rotation
```

Rotation values:
- **0** = Portrait (0°) - 240x320
- **1** = Landscape (90°) - 320x240
- **2** = Portrait inverted (180°) - 240x320
- **3** = Landscape inverted (270°) - 320x240

This project uses **rotation 0** for portrait mode.

## Touch Calibration

The touch screen is pre-calibrated for standard CYD units:

```cpp
int x = map(p.x, 200, 3700, 0, 240);  // Raw to screen X
int y = map(p.y, 240, 3800, 0, 320);  // Raw to screen Y
```

**Raw touch range:**
- X: ~200 to ~3700
- Y: ~240 to ~3800

These values work for most CYD units. If your touch is inaccurate, adjust the min/max values based on corner readings.

## Code Initialization Sequence

### Display Init
```cpp
tft.init();              // Initialize ILI9341
tft.setRotation(0);      // Set portrait mode
tft.fillScreen(BLACK);   // Clear screen
```

### Touch Init (Critical!)
```cpp
// Create separate SPI instance for touch
SPIClass touchSPI = SPIClass(HSPI);

// Initialize with touch-specific pins
touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);

// Pass separate SPI bus to touch library
ts.begin(touchSPI);
ts.setRotation(0);
```

**Important:** You MUST create and pass a separate SPI instance to the touch controller!

## Common CYD Variants

### Standard CYD (Most Common)
- Part: ESP32-2432S028
- Display: ILI9341
- Touch: XPT2046
- This configuration works ✓

### CYD with RGB LED
- Same as standard
- Has RGB LED on GPIO 4
- LED shares pin with TFT_RST (be careful!)

### CYD with LDR
- Light sensor on GPIO 34
- Can be used for auto-brightness

### CYD with SD Card
- SD pins: MOSI=23, MISO=19, CLK=18, CS=5
- Not used in this project

## Power Requirements

- **Input:** 5V via USB-C or GPIO VIN
- **Display:** ~50mA @ 5V
- **ESP32:** ~80mA active, ~10mA WiFi TX
- **Total:** ~150mA typical, 250mA peak

## GPIO Usage

**Used by this project:**
- GPIO 2, 12, 13, 14, 15, 21 - Display
- GPIO 25, 32, 33, 36, 39 - Touch
- GPIO 26 - Buzzer (P4 speaker connector)
- GPIO 1, 3 - Serial (USB)

**Available for expansion:**
- GPIO 0, 4, 16, 17, 22, 23, 27, 34, 35

**Avoid (strapping pins):**
- GPIO 0 - Boot mode
- GPIO 2 - Boot mode (used by display DC)
- GPIO 12 - Flash voltage (used by display MISO)
- GPIO 15 - Boot mode (used by display CS)

## Troubleshooting Hardware Issues

### Display Shows Nothing
- Check backlight (GPIO 21 should be HIGH)
- Verify SPI connections
- Try toggling TFT_INVERSION_ON/OFF
- Check if using ILI9341_2_DRIVER

### Touch Not Working
- Verify separate SPI bus initialized
- Check TOUCH_CS=33 is defined
- Ensure ts.begin(touchSPI) gets separate SPI instance
- Use firm pressure (resistive touch)

### Random Touch Events
- Normal during boot (cleared automatically)
- Check for loose connections
- Verify TOUCH_IRQ pin connected

### Colors Inverted
- Toggle TFT_INVERSION_ON/OFF in platformio.ini

### Display Sideways
- Change tft.setRotation() value (0-3)
- Match ts.setRotation() to display

## References

- [CYD GitHub](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [TFT_eSPI Documentation](https://github.com/Bodmer/TFT_eSPI)
- [XPT2046 Library](https://github.com/PaulStoffregen/XPT2046_Touchscreen)

## Board Purchase

Search for **ESP32-2432S028** on:
- AliExpress
- Amazon
- eBay

Typical price: $10-15 USD

---

**This configuration has been tested and verified working on standard CYD hardware.** ✓
