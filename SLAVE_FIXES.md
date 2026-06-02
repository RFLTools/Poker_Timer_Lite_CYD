# Slave Device Synchronization Fixes

## Issues Fixed

### 1. ✅ Slave Not Receiving Broadcasts

**Problem:** Slave devices showed red "X" (disconnected) instead of green "S" (connected)

**Root Cause:** Only the master was adding the broadcast peer. Slaves need to add the broadcast peer as well to receive broadcasts properly.

**Fix:** Modified `initESPNow()` (main.cpp:1521-1541)
- Both master and slave now add broadcast peer
- Slaves can now receive broadcasts from master
- Slaves can send commands to master via broadcast address

```cpp
// Add broadcast peer (both master and slave need this)
esp_now_peer_info_t peerInfo = {};
memcpy(peerInfo.peer_addr, broadcastAddress, 6);
peerInfo.channel = 0;  // Use channel 0 for auto
peerInfo.encrypt = false;

if (esp_now_add_peer(&peerInfo) != ESP_OK) {
  Serial.println("✗ Failed to add broadcast peer");
} else {
  // Success message for both master and slave
}
```

### 2. ✅ Slave Display Not Updating

**Problem:** Slave always showed "PAUSED" even when master was running

**Root Cause:** Display updates were throttled too aggressively, and state changes weren't being detected

**Fix:** Enhanced `handleTimerStateMessage()` (main.cpp:1622-1656)
- Added state change detection
- Immediate display update on state change
- Added detailed logging to show when slave receives updates
- Better throttling (1 second instead of 500ms)

```cpp
bool stateChanged = (msg->currentRound != lastRound) || 
                    (msg->remainingSeconds != lastSeconds) ||
                    (msg->timerRunning != lastRunning);

if (stateChanged) {
  Serial.print("Slave RX: Round ");
  Serial.print(msg->currentRound + 1);
  // ... detailed logging
  
  lastRound = msg->currentRound;
  lastSeconds = msg->remainingSeconds;
  lastRunning = msg->timerRunning;
}

// Update local state
currentRound = msg->currentRound;
remainingSeconds = msg->remainingSeconds;
timerRunning = msg->timerRunning;

// Update display immediately on state change
if (stateChanged || (millis() - lastDisplayUpdate > 1000)) {
  lastDisplayUpdate = millis();
  drawTimerDisplay();
}
```

### 3. ✅ NEXT/PREV Buttons Greyed Out on Slaves

**Problem:** NEXT/PREV buttons appeared functional on slaves but shouldn't be

**Fix:** Modified `drawTimerDisplay()` (main.cpp:956-971)
- NEXT/PREV buttons now display in grey on slave devices
- Visual indicator that they're not functional
- START/PAUSE button remains active (green/red)

```cpp
// Grey out NEXT/PREV on slave devices (not functional)
if (deviceMode == MODE_SLAVE) {
  btnNext.color = TFT_DARKGREY;
  btnNext.textColor = 0x8410;  // Light grey color (RGB565)
  btnPrev.color = TFT_DARKGREY;
  btnPrev.textColor = 0x8410;
} else {
  btnNext.color = TFT_BLUE;
  btnNext.textColor = TFT_WHITE;
  btnPrev.color = TFT_ORANGE;
  btnPrev.textColor = TFT_BLACK;
}
```

### 4. ✅ NEXT/PREV Buttons Disabled on Slaves

**Fix:** Added checks in `handleTouch()` (main.cpp:1282-1287, 1365-1370)
- NEXT button returns early if pressed on slave
- PREV button returns early if pressed on slave
- Logs message for debugging
- No confirmation dialog shown

```cpp
// Check Next button
else if (isTouchInButton(x, y, btnNext)) {
  // NEXT button disabled on slaves
  if (deviceMode == MODE_SLAVE) {
    Serial.println("Next pressed on slave - ignoring (button disabled)");
    return;
  }
  // ... continue with normal handling
}
```

### 5. ✅ START Button Remains Active on Slaves

**Behavior:** 
- START/PAUSE button fully functional on slaves
- Sends CMD_START_PAUSE to master
- Allows slave to control timer
- Enables config mode access when paused

**Why:** Config mode is only accessible when timer is paused (via gear icon). Keeping START button active allows slaves to:
1. Pause the timer remotely
2. Access config mode when paused (though config will still need to be done on master)

### 6. ✅ Enhanced Debugging

**Added Logging:**

**Master (main.cpp:1589-1604):**
```cpp
// Logs every 5 seconds
Serial.print("Master TX: Round ");
Serial.print(currentRound + 1);
Serial.print(", Time ");
Serial.print(remainingSeconds);
Serial.print("s, ");
Serial.print(timerRunning ? "RUNNING" : "PAUSED");
Serial.print(" - Send ");
Serial.println(result == ESP_OK ? "OK" : "FAILED");
```

**Slave (main.cpp:1633-1643):**
```cpp
// Logs on state change
Serial.print("Slave RX: Round ");
Serial.print(msg->currentRound + 1);
Serial.print(", Time ");
Serial.print(msg->remainingSeconds);
Serial.print("s, ");
Serial.println(msg->timerRunning ? "RUNNING" : "PAUSED");
```

**Message Size Validation (main.cpp:1557-1561):**
```cpp
if (data_len != sizeof(TimerStateMessage)) {
  Serial.print("Slave: Timer state message size mismatch. Expected ");
  Serial.print(sizeof(TimerStateMessage));
  Serial.print(", got ");
  Serial.println(data_len);
}
```

## Testing Checklist

### Master Device:
- [ ] Shows `M:0S` when no slaves connected
- [ ] Shows `M:1S` when one slave connected
- [ ] Serial monitor shows `Master TX:` logs every 5 seconds
- [ ] All buttons (START/PAUSE/NEXT/PREV) work normally
- [ ] Config gear icon accessible when paused

### Slave Device:
- [ ] Shows green `S` when connected to master
- [ ] Shows red `X` when master is off or out of range
- [ ] Serial monitor shows `Slave RX:` logs when receiving updates
- [ ] Display updates immediately when master changes state
- [ ] Timer shows "RUNNING" when master is running
- [ ] Timer shows "PAUSED" when master is paused
- [ ] Remaining time matches master
- [ ] Current round matches master
- [ ] NEXT button greyed out (dark grey with light grey text)
- [ ] PREV button greyed out (dark grey with light grey text)
- [ ] START/PAUSE button active (green when paused, red when running)
- [ ] Pressing greyed-out NEXT/PREV has no effect
- [ ] Pressing START/PAUSE controls master timer

### Synchronization:
- [ ] Master starts timer → Slave updates to "RUNNING" within 1 second
- [ ] Master pauses timer → Slave updates to "PAUSED" within 1 second
- [ ] Master advances round → Slave advances round within 1 second
- [ ] Slave presses START → Master starts/pauses
- [ ] Multiple slaves sync simultaneously
- [ ] Slave reconnects automatically if master restarts

## Serial Monitor Output Examples

### Successful Sync - Master:
```
--- Initializing ESPNow ---
MAC Address: AA:BB:CC:DD:EE:FF
✓ ESPNow initialized successfully
✓ Broadcast peer added (Master will broadcast to all slaves)
Device mode: MASTER
---------------------------

Master TX: Round 1, Time 900s, RUNNING - Send OK
New slave connected: 11:22:33:44:55:66 (Total slaves: 1)
Master TX: Round 1, Time 895s, RUNNING - Send OK
```

### Successful Sync - Slave:
```
--- Initializing ESPNow ---
MAC Address: 11:22:33:44:55:66
✓ ESPNow initialized successfully
✓ Broadcast peer added (Slave can send to master and receive broadcasts)
Device mode: SLAVE
---------------------------

Slave requesting full sync from master...
Slave sent command: 4
Slave RX: Round 1, Time 900s, RUNNING
Slave RX: Round 1, Time 899s, RUNNING
Slave RX: Round 1, Time 898s, RUNNING
```

### Slave Button Press:
```
Start/Pause pressed
Slave: Sent start/pause command to master
```

### Master Receives Command:
```
Master received command from slave 11:22:33:44:55:66: 1
Timer paused by slave
Master TX: Round 1, Time 850s, PAUSED - Send OK
```

## Troubleshooting

### Slave Shows Red X
1. Check master is powered on and in Master mode
2. Check both devices show ESPNow initialized in serial monitor
3. Verify both devices on same WiFi channel (should auto-negotiate)
4. Check broadcast peer was added successfully on both devices

### Slave Not Updating
1. Check master serial monitor shows `Master TX:` logs
2. Check slave serial monitor shows `Slave RX:` logs
3. Verify message size matches (should be 8 bytes for TimerStateMessage)
4. Check for "Send OK" in master logs
5. Restart both devices

### Slave Buttons Not Working
1. Grey buttons (NEXT/PREV) should NOT work - this is correct behavior
2. START/PAUSE button should work - check serial monitor for command send
3. Check master receives command in serial monitor

## File Changes Summary

| File | Lines Modified | Description |
|------|----------------|-------------|
| main.cpp:1521-1541 | initESPNow() | Add broadcast peer for both master and slave |
| main.cpp:1550-1575 | onESPNowDataRecv() | Add debug logging for message size |
| main.cpp:1589-1604 | broadcastTimerState() | Add periodic logging and error checking |
| main.cpp:1622-1656 | handleTimerStateMessage() | Enhanced state change detection and logging |
| main.cpp:956-971 | drawTimerDisplay() | Grey out NEXT/PREV on slaves |
| main.cpp:1282-1287 | handleTouch() | Disable NEXT button on slaves |
| main.cpp:1365-1370 | handleTouch() | Disable PREV button on slaves |

## Build and Flash

```bash
# Compile
pio run

# Flash to master device
pio run --target upload

# Flash to slave device  
pio run --target upload

# Monitor serial output
pio device monitor -b 115200
```

---

**All fixes implemented and ready for testing!**
