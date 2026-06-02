# Slave Behavior Update - Display Only Mode

## Overview

**Slaves are now display-only devices** that mirror the master's timer state. All control functions removed from slaves.

---

## Changes Implemented

### 1. ✅ Slave Heartbeat System

**Previous:** Slaves were only detected when sending control commands  
**Now:** Slaves send heartbeat messages every 5 seconds

**Implementation:**
- New message type: `MSG_HEARTBEAT`
- New structure: `HeartbeatMessage` (2 bytes)
- Function: `sendHeartbeat()` - sends heartbeat to master
- Function: `handleHeartbeatMessage()` - master receives and tracks slave
- Loop: Slaves send heartbeat every 5 seconds (main.cpp:2064-2070)

**Result:**
- Master now shows correct slave count (e.g., `M:2S` for 2 slaves)
- Slaves tracked automatically without user interaction
- Master logs heartbeat receipt every 10 seconds

---

### 2. ✅ Slave START Button Behavior

**Previous:** START button paused/resumed the master timer  
**Now:** START button toggles config gear icon visibility

**Why:** Config mode is only accessible via the gear icon. On slaves, the START button toggles the gear icon so you can access config mode to change the device mode back to Master or Standalone.

**Implementation:**
- New global variable: `slaveShowConfigGear` (bool)
- Modified `handleTouch()` for START button (main.cpp:1246-1267)
- On slave: Toggles `slaveShowConfigGear` flag
- On master/standalone: Works normally (pause/resume)

**Behavior:**
1. Slave shows timer synced from master
2. Press START → Config gear appears
3. Can now tap gear to enter config mode
4. Press START again → Config gear disappears

---

### 3. ✅ Slave Display Shows "SLAVE"

**Previous:** Slaves showed "PAUSED" or "RUNNING"  
**Now:** Slaves always show "SLAVE" in red

**Implementation:**
- Modified `drawTimerDisplay()` status section (main.cpp:1023-1039)
- Slaves: Always display "SLAVE" (red text)
- Master/Standalone: Shows "RUNNING" (green) or "PAUSED" (red)

**Visual:**
```
Master:    RUNNING (green) or PAUSED (red)
Slave:     SLAVE (red)
```

---

### 4. ✅ Config Gear Toggle on Slaves

**Previous:** Gear only shown when timer paused (not on slaves)  
**Now:** Gear shown when `slaveShowConfigGear` is true

**Implementation:**
- Modified config gear display logic (main.cpp:1061-1085)
- Slave: Show gear when `slaveShowConfigGear == true`
- Master/Standalone: Show gear when `!timerRunning`

**Usage:**
1. Slave device running, showing master's timer
2. Press START button → Gear appears
3. Tap gear → Enter config mode
4. Change device mode or configure tournament
5. After reboot, press START to hide gear again

---

### 5. ✅ Removed Slave Control Commands

**Previous:** Slaves could send START/PAUSE/NEXT/PREV commands to master  
**Now:** Slaves are display-only, no control over master

**Removed:**
- `enum ControlCommand` - no longer needed
- `struct ControlCommandMessage` - no longer needed
- `MSG_CONTROL_CMD` message type - no longer used
- `sendControlCommand()` function - deleted
- `handleControlCommandMessage()` function - deleted
- All slave command sending code

**Retained:**
- NEXT/PREV buttons still greyed out on slaves
- NEXT/PREV button handlers still disabled on slaves
- START button repurposed for gear toggle

---

## Display Reference

### Master Device

**Top-left:** `M:2S` (Master with 2 slaves connected)  
**Buttons:**
- START (green) → Start timer
- PAUSE (red) → Pause timer  
- NEXT (blue) → Skip to next round
- PREV (orange) → Go to previous round

**Status:** `RUNNING` (green) or `PAUSED` (red)  
**Gear Icon:** Shown when paused

---

### Slave Device

**Top-left:** `S` (green - connected) or `X` (red - disconnected)  
**Buttons:**
- START (green/red) → Toggle config gear visibility
- NEXT (dark grey) → Disabled
- PREV (dark grey) → Disabled

**Status:** `SLAVE` (red)  
**Gear Icon:** Shown when START button pressed (toggles)

---

## Serial Monitor Output

### Master Logs

```
--- Initializing ESPNow ---
MAC Address: AA:BB:CC:DD:EE:FF
✓ ESPNow initialized successfully
✓ Broadcast peer added (Master will broadcast to all slaves)
Device mode: MASTER

Master TX: Round 1, Time 900s, RUNNING - Send OK
Master: Heartbeat from 11:22:33:44:55:66 - Total slaves: 1
Master: Heartbeat from 22:33:44:55:66:77 - Total slaves: 2
Master TX: Round 1, Time 895s, RUNNING - Send OK
```

### Slave Logs

```
--- Initializing ESPNow ---
MAC Address: 11:22:33:44:55:66
✓ ESPNow initialized successfully
✓ Broadcast peer added (Slave can send to master and receive broadcasts)
Device mode: SLAVE

Slave: Heartbeat sent to master
Slave RX: Round 1, Time 900s, RUNNING
Slave RX: Round 1, Time 899s, RUNNING
Start/Pause pressed
Slave: Config gear SHOWN
Start/Pause pressed
Slave: Config gear HIDDEN
```

---

## Testing Checklist

### Master Device Tests
- [ ] Shows `M:0S` when no slaves connected
- [ ] Shows `M:1S` after one slave sends heartbeat
- [ ] Shows `M:2S` after two slaves send heartbeats
- [ ] Slave count decreases when slave powered off
- [ ] Serial shows "Heartbeat from..." every 10 seconds
- [ ] Serial shows "Master TX..." every 5 seconds
- [ ] All buttons work normally
- [ ] Gear icon shows when paused
- [ ] Timer runs normally

### Slave Device Tests
- [ ] Shows green `S` when receiving broadcasts from master
- [ ] Shows red `X` when master is off
- [ ] Display updates match master (round, time, running state)
- [ ] Serial shows "Slave RX..." when state changes
- [ ] Serial shows "Heartbeat sent" every 10 seconds
- [ ] Status always shows "SLAVE" in red
- [ ] Press START → Gear icon appears
- [ ] Press START again → Gear icon disappears
- [ ] NEXT button greyed out and non-functional
- [ ] PREV button greyed out and non-functional
- [ ] Can enter config mode when gear shown

### Synchronization Tests
- [ ] Master starts timer → Slave shows RUNNING state
- [ ] Master pauses timer → Slave shows PAUSED state  
- [ ] Master advances round → Slave advances round
- [ ] Slave timer countdown matches master
- [ ] Multiple slaves all show same state
- [ ] Slave reconnects when master restarts

---

## Code Changes Summary

| File | Function | Change |
|------|----------|---------|
| main.cpp:64-66 | Message types | Changed `MSG_CONTROL_CMD` to `MSG_HEARTBEAT` |
| main.cpp:68-71 | Structures | Replaced `ControlCommandMessage` with `HeartbeatMessage` |
| main.cpp:98 | Global vars | Added `slaveShowConfigGear` flag |
| main.cpp:183-189 | Prototypes | Replaced control functions with heartbeat functions |
| main.cpp:1023-1039 | drawTimerDisplay() | Slaves show "SLAVE" instead of PAUSED |
| main.cpp:1061-1085 | drawTimerDisplay() | Gear toggle logic for slaves |
| main.cpp:1246-1267 | handleTouch() | START button toggles gear on slaves |
| main.cpp:1618-1636 | broadcastRoundConfig() | Added `sendHeartbeat()` function |
| main.cpp:1665-1675 | handleHeartbeatMessage() | New handler for slave heartbeats |
| main.cpp:1675 | - | **DELETED** `sendControlCommand()` |
| main.cpp:1732-1792 | - | **DELETED** `handleControlCommandMessage()` |
| main.cpp:1888-1895 | setup() | **REMOVED** slave sync request on startup |
| main.cpp:2064-2070 | loop() | Added slave heartbeat sending every 5 seconds |

---

## Migration Guide

### If You Have Existing Devices

1. **Flash new firmware** to all devices
2. **Master devices:** No config changes needed - will work immediately
3. **Slave devices:** Behavior changed:
   - Can no longer control master timer
   - START button now toggles config gear
   - Always shows "SLAVE" in red

### Changing Device Modes

**To access config on slave:**
1. Press START button → Gear appears
2. Tap gear icon → Enter config mode
3. Change "Device Sync Mode" to Master or Standalone
4. Save → Device reboots with new mode

---

## Troubleshooting

### Master Shows M:0S (No Slaves Detected)

**Check:**
1. Slave device powered on?
2. Slave in Slave mode (not Standalone)?
3. Slave serial shows "Heartbeat sent"?
4. Both devices within range (~50m)?

**Fix:**
- Wait 5 seconds for first heartbeat
- Check slave serial monitor for ESPNow init success
- Restart both devices

### Slave Shows Red X (Disconnected)

**Check:**
1. Master powered on and running?
2. Master in Master mode?
3. Master serial shows "Master TX:"?
4. Devices within range?

**Fix:**
- Verify master is broadcasting (check serial)
- Move devices closer
- Restart master first, then slave

### Slave Not Updating

**Check:**
1. Slave shows green `S`?
2. Master serial shows broadcasts?
3. Slave serial shows "Slave RX:"?

**Fix:**
- Restart both devices
- Check message size matches (8 bytes)
- Verify both have same firmware version

---

## Benefits of New Approach

✅ **Simpler slave operation** - No confusing control buttons  
✅ **Clear visual feedback** - "SLAVE" always visible  
✅ **Master always detects slaves** - Heartbeat system  
✅ **Config still accessible** - Via START button toggle  
✅ **Less network traffic** - No control commands  
✅ **Clearer device roles** - Master controls, slaves display  

---

**All changes implemented and ready for testing!**

Compile and flash to test the new slave behavior.
