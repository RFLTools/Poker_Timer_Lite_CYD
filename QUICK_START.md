# Quick Start Guide

## 1. Upload to CYD

```bash
# Using PlatformIO
pio run --target upload
```

Or click **Upload** in PlatformIO toolbar.

## 2. First Power On

The timer will:
- Initialize display
- Load default tournament settings
- Show timer screen immediately

**Default settings:**
- 25 rounds, 15 minutes each
- Blinds start at 25/50
- Breaks every 4th round

## 3. Using the Timer

### Touch Controls

```
┌────────────────────┐
│ Round 1      [⚙️]  │ ← Tap gear to configure
├────────────────────┤
│ Blinds: 25/50      │
│      15:00         │
│     RUNNING        │
├────────────────────┤
│ [START][NEXT][PREV]│ ← Touch buttons
└────────────────────┘
```

**Buttons:**
- **START** - Start timer
- **PAUSE** - Pause timer (when running, START becomes PAUSE)
- **NEXT** - Skip to next round
- **PREV** - Go back to previous round
- **⚙️ Gear** - Configure tournament

### Normal Operation

1. **Tap START** - Timer begins counting down
2. When round finishes - Automatically advances to next round
3. **Tap PAUSE** - Pause anytime
4. Use **NEXT/PREV** to manually navigate rounds

## 4. Configure Your Tournament

**To customize blinds and rounds:**

### Step 1: Enter Config Mode
- Tap the **⚙️ gear icon** (top-right corner)

### Step 2: Confirm
- Screen shows: "Enter Config Mode?"
- Tap **YES** (green button)
- Or tap **NO** (red button) to cancel

### Step 3: Connect to WiFi
- CYD creates WiFi access point
- On your phone/computer:
  - Open WiFi settings
  - Connect to: **PokerTimer**
  - No password required

### Step 4: Open Web Interface
- Open browser
- Go to: **http://192.168.4.1**
- Configuration page loads

### Step 5: Edit Settings
For each round you can set:
- **Duration** (minutes)
- **Small Blind**
- **Big Blind**
- **Ante**
- **Break checkbox** - Mark rounds as breaks

### Step 6: Save
- Click **Save** button
- Device reboots with new settings
- Returns to timer screen

## 5. Tips & Tricks

### Auto Features
- Timer auto-saves every 10 seconds
- Settings persist after power loss
- Can resume from last position on restart

### Resume After Power Loss
When powered on after unexpected shutdown:
- Shows saved state
- Touch **LEFT** side to resume
- Touch **RIGHT** side to start fresh
- Or wait 10 seconds - auto-resumes

### Timer State
All of this is saved automatically:
- Current round
- Time remaining
- Running/paused state
- All tournament settings

## Troubleshooting

### Touch Not Working
- Use **firm pressure** (resistive touch)
- Check Serial Monitor at 115200 baud
- Look for touch coordinate output

### Can't Enter Config Mode
- Gear icon is **top-right corner**
- Must tap **YES** to confirm
- 10-second timeout returns to timer

### Timer Doesn't Advance
- Check if paused (shows "PAUSED" in red)
- Tap START to resume

### Config Page Won't Load
1. Verify connected to **PokerTimer** WiFi
2. Try: **192.168.4.1** or **http://192.168.4.1**
3. Check Serial Monitor for AP IP address

## Serial Monitor

Connect at **115200 baud** to see:
- Initialization progress
- Touch events
- Round changes
- Debug information

Useful for troubleshooting!

## Next Steps

- Customize your tournament structure
- Test with a practice tournament
- Save different configurations for different game types

**Enjoy your poker timer!** 🎰
