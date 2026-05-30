# Configuration Mode

## How to Enter Config Mode

### Step 1: Look for Gear Icon
On the main timer screen, you'll see a small gear icon in the top-right corner (gray button with gear symbol).

### Step 2: Tap the Gear Icon
Touch the gear icon button. You'll see a confirmation screen:

```
Enter Config Mode?

This will start WiFi AP
and pause the timer.

[YES]  [NO]
```

### Step 3: Confirm
- Tap **YES** (green button) to enter config mode
- Tap **NO** (red button) to cancel and return to timer
- Wait 10 seconds - automatically cancels and returns to timer

### Step 4: Configure
Once in config mode:
1. Connect to WiFi network: **PokerTimer**
2. Open browser: **http://192.168.4.1**
3. Configure your tournament
4. Click Save - device reboots

## Benefits

✅ **No more accidental config mode** - requires intentional button press
✅ **Enter config anytime** - don't have to restart device
✅ **Confirmation dialog** - prevents accidental taps
✅ **Timer pauses** - won't lose time while configuring
✅ **Auto-timeout** - if you don't confirm in 10 seconds, returns to timer

## Screen Layout

```
┌────────────────────────────┐
│ Round 1          [⚙️ GEAR] │  ← Config button here
├────────────────────────────┤
│ Blinds:                    │
│ 25/50                      │
│                            │
│      15:00                 │  ← Timer
│                            │
│     RUNNING                │  ← Status
├────────────────────────────┤
│ [START] [NEXT] [PREV]      │  ← Control buttons
└────────────────────────────┘
```

## Troubleshooting

### Can't See Gear Icon
- Should be in top-right corner, small gray button
- Check if display is fully initializing
- Look for small gear/cog symbol

### Gear Button Not Responding
- Touchscreen calibration may be off
- Watch Serial Monitor for "Touch at: X, Y" when you tap
- Should be around X=200-230, Y=5-35
- See touch calibration section in main README

### Still Entering Config Mode on Startup
- Old code may still be running
- Make sure you uploaded the NEW version
- Check Serial Monitor - should NOT see "Checking for config mode touch..."
- Should only see "Tap gear icon (top-right) to configure timer"

### YES/NO Buttons Not Working
- Touch calibration issue
- Serial Monitor shows touch coordinates
- YES button: X=30-110, Y=220-270
- NO button: X=130-210, Y=220-270

## For Developers

### Config Button Definition
```cpp
TouchButton btnConfig = {200, 5, 30, 30, "CFG", TFT_DARKGREY, TFT_WHITE};
```

### Confirmation Dialog Code
Located in `handleTouch()` function around line 817. Shows YES/NO dialog with 10-second timeout.

### Removed Code
- Removed all startup touch detection (lines that checked for sustained touch during boot)
- Removed "Touch & HOLD screen for Config Mode" message
- Simplified setup() function

## Migration from Old Version

If upgrading from the old touch-at-startup version:

1. **Upload new code** - no changes to platformio.ini needed
2. **Power cycle device** - should boot directly to timer (no config check)
3. **Look for gear icon** - top-right corner
4. **Test config mode** - tap gear, confirm YES
5. **Done!** - much better UX

No settings are lost - all your tournament configuration is preserved in NVS flash memory.

## Future Enhancements

Possible improvements:
- Long-press gear icon to bypass confirmation
- Different gear icon colors to indicate WiFi status
- Settings shortcut (tap gear while timer paused = quick settings)
- Export/import configuration via QR code

---

**This is the recommended config mode method going forward!** 🎯
