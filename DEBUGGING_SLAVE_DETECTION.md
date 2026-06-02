# Debugging Slave Detection Issues

## Problem: Master shows M:0S instead of M:1S or M:2S

The master should show the number of connected slaves (e.g., M:2S for 2 slaves), but shows M:0S even when slaves are running.

---

## Enhanced Debugging (Added)

I've added extensive debug logging to help identify the issue:

### 1. Slave Heartbeat Logging

**Location:** `sendHeartbeat()` function

**What it logs:**
```
Slave: Heartbeat sent (2 bytes) - OK
```
or
```
Slave: Heartbeat sent (2 bytes) - FAILED
```

**What to check:**
- Does it say "OK" or "FAILED"?
- Is it being called every 5 seconds?
- Is the size showing "2 bytes"?

### 2. Master Receive Logging

**Location:** `onESPNowDataRecv()` function

**What it logs:**
```
Master: Received MSG_HEARTBEAT, size 2 (expected 2)
Master: Processing heartbeat from AA:BB:CC:DD:EE:FF
```
or if size mismatch:
```
Master: Received MSG_HEARTBEAT, size 3 (expected 2)
Master: Heartbeat size mismatch - ignored
```

**What to check:**
- Is master receiving MSG_HEARTBEAT at all?
- Does the size match (should be 2 bytes)?
- Is it showing the slave's MAC address?

### 3. Slave Tracking Logging

**Location:** `updateSlaveTracking()` function

**What it logs:**
```
updateSlaveTracking called for AA:BB:CC:DD:EE:FF
  -> NEW SLAVE! Total slaves: 1
```
or for existing slave:
```
updateSlaveTracking called for AA:BB:CC:DD:EE:FF
  -> Existing slave, timestamp updated
```

**What to check:**
- Is this function being called?
- Does it say "NEW SLAVE" or "Existing slave"?
- What is the total slave count?

---

## Diagnostic Steps

### Step 1: Check Slave is Sending

**On Slave Serial Monitor (115200 baud):**

Look for:
```
--- Initializing ESPNow ---
MAC Address: XX:XX:XX:XX:XX:XX
✓ ESPNow initialized successfully
✓ Broadcast peer added (Slave can send to master and receive broadcasts)
Device mode: SLAVE
```

Then every 5 seconds:
```
Slave: Heartbeat sent (2 bytes) - OK
```

**Issues to check:**
- ❌ "Heartbeat sent - FAILED" → ESPNow not initialized properly
- ❌ No heartbeat messages → deviceMode not SLAVE or espNowInitialized is false
- ❌ Wrong size → HeartbeatMessage structure issue

### Step 2: Check Master is Receiving

**On Master Serial Monitor (115200 baud):**

Look for:
```
--- Initializing ESPNow ---
MAC Address: YY:YY:YY:YY:YY:YY
✓ ESPNow initialized successfully
✓ Broadcast peer added (Master will broadcast to all slaves)
Device mode: MASTER
```

Then when slave sends heartbeat:
```
Master: Received MSG_HEARTBEAT, size 2 (expected 2)
Master: Processing heartbeat from XX:XX:XX:XX:XX:XX
updateSlaveTracking called for XX:XX:XX:XX:XX:XX
  -> NEW SLAVE! Total slaves: 1
```

**Issues to check:**
- ❌ No "Received MSG_HEARTBEAT" → Master not receiving messages from slave
- ❌ "size X (expected 2)" where X ≠ 2 → Message corruption or structure mismatch
- ❌ "NEW SLAVE" not showing → updateSlaveTracking not incrementing count

### Step 3: Check Display Update

**On Master Display:**

Should show:
```
M:1S  (top-left corner)
```

If showing:
```
M:0S  (top-left corner)
```

**Check:**
```cpp
int getActiveSlaveCount() {
  return activeSlaveCount;  // Should return 1
}
```

---

## Common Issues and Fixes

### Issue 1: Slave sends OK but master doesn't receive

**Possible causes:**
1. Devices on different WiFi channels
2. Devices out of range (>50m indoors)
3. Master's ESPNow receiver callback not registered

**Fix:**
- Move devices within 5m of each other for testing
- Restart both devices in this order: Master first, then slave
- Check master serial for "esp_now_register_recv_cb" success

### Issue 2: Master receives but size mismatch

**Possible causes:**
1. Different firmware versions on master and slave
2. Structure alignment issues
3. Message corruption

**Fix:**
- Reflash both devices with same firmware
- Check sizeof(HeartbeatMessage) on both (should be 2)
- Add padding to structure if needed

### Issue 3: updateSlaveTracking called but count stays 0

**Possible causes:**
1. freeSlot = -1 (no free slots)
2. activeSlaveCount not incrementing
3. Display not updating

**Fix:**
- Check "ERROR: No free slot" message
- Check MAX_TRACKED_SLAVES (should be 10)
- Force display update by pressing a button

### Issue 4: Count increments but display shows 0

**Possible causes:**
1. getActiveSlaveCount() not called
2. Display update not happening
3. Wrong variable being displayed

**Fix:**
- Add Serial.print in getActiveSlaveCount()
- Force redraw by pressing START button
- Check drawTimerDisplay() is calling getActiveSlaveCount()

---

## Message Size Reference

```cpp
struct HeartbeatMessage {
  uint8_t messageType;  // 1 byte
  uint8_t slaveId;      // 1 byte
};
// Total: 2 bytes
```

**Expected in logs:**
- Slave: "Heartbeat sent (2 bytes)"
- Master: "size 2 (expected 2)"

If you see different sizes, there's a structure mismatch between master and slave firmware.

---

## Testing Procedure

### 1. Start Master Device

**Serial output should show:**
```
✓ ESPNow initialized successfully
Device mode: MASTER
Master TX: Round 1, Time 900s, RUNNING - Send OK
```

**Display should show:**
```
M:0S (no slaves yet)
```

### 2. Start Slave Device

**Slave serial should show:**
```
✓ ESPNow initialized successfully
Device mode: SLAVE
Slave: Heartbeat sent (2 bytes) - OK
```

**Master serial should show:**
```
Master: Received MSG_HEARTBEAT, size 2 (expected 2)
Master: Processing heartbeat from AA:BB:CC:DD:EE:FF
updateSlaveTracking called for AA:BB:CC:DD:EE:FF
  -> NEW SLAVE! Total slaves: 1
```

**Master display should update to:**
```
M:1S
```

### 3. Verify Ongoing Heartbeats

**Every 5 seconds, slave sends:**
```
Slave: Heartbeat sent (2 bytes) - OK
```

**Master should occasionally log:**
```
updateSlaveTracking called for AA:BB:CC:DD:EE:FF
  -> Existing slave, timestamp updated
```

---

## Quick Checklist

When slave not detected:

- [ ] Slave serial shows "Heartbeat sent (2 bytes) - OK"
- [ ] Master serial shows "Received MSG_HEARTBEAT"
- [ ] Master serial shows "Processing heartbeat from..."
- [ ] Master serial shows "updateSlaveTracking called"
- [ ] Master serial shows "NEW SLAVE! Total slaves: 1"
- [ ] Master display updates to show "M:1S"
- [ ] Devices within 10m of each other
- [ ] Both devices show ESPNow initialized successfully
- [ ] Both devices on same firmware version

---

## Advanced Debugging

If all logs look correct but count still shows 0, add this to your code temporarily:

**In drawTimerDisplay() where it shows slave count:**

```cpp
// Temporary debugging
Serial.print("Display update: activeSlaveCount = ");
Serial.print(activeSlaveCount);
Serial.print(", getActiveSlaveCount() = ");
Serial.println(getActiveSlaveCount());
```

This will confirm if the count is correct but display is wrong, or if count itself is 0.

---

## File This Information

When reporting the issue, please provide:

1. **Slave Serial Monitor Log** (first 30 seconds)
2. **Master Serial Monitor Log** (first 30 seconds)
3. **Screenshot of master display** showing M:0S
4. **Distance between devices**
5. **Firmware version/date**

With the enhanced logging, we should be able to pinpoint exactly where the detection is failing.
