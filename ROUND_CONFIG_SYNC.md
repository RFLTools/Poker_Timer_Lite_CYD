# Round Configuration Synchronization

## Problem Solved

**Issue:** Slaves were displaying blinds/antes from their OWN local configuration, not the master's configuration.

**Example of the problem:**
- Master configured with blinds: 100/200, 200/400, 300/600
- Slave configured with default blinds: 25/50, 50/100, 75/150
- Master shows "Round 2: 200/400"
- Slave receives "Round 2" → looks up local round 2 → shows "50/100" ❌

**Solution:** Master now broadcasts ALL round configurations to slaves automatically.

---

## How It Works

### 1. When Slave Connects

1. Slave sends heartbeat to master
2. Master receives heartbeat → adds slave to tracking
3. Slave marked as `configSynced = false` (needs config)
4. Within 2 seconds, master broadcasts all 25 round configs
5. Slave receives and stores all round configs
6. Slave marked as `configSynced = true`

**Result:** Slave now has identical configuration to master

### 2. When Master Config Changes

1. User changes config on master via web interface
2. Master saves config to preferences
3. **BEFORE rebooting:** Master broadcasts all round configs to slaves
4. Slaves receive and update their configurations
5. Master reboots

**Result:** All slaves automatically updated with new config

### 3. Continuous Sync

- Master checks every 2 seconds for slaves needing config
- New slaves automatically get config within 2 seconds
- No user interaction needed

---

## Implementation Details

### New Slave Tracking Field

```cpp
struct SlaveInfo {
  uint8_t macAddress[6];
  unsigned long lastSeen;
  bool active;
  bool configSynced;  // NEW: Have we sent round configs to this slave?
};
```

### New Functions

**`broadcastAllRoundConfigs()`** (main.cpp:1646-1657)
- Broadcasts all 25 round configs sequentially
- 20ms delay between messages to avoid flooding
- Logs to serial monitor

**`syncSlaveConfigs()`** (main.cpp:1659-1680)
- Checks if any slaves need config sync
- If yes, broadcasts all configs
- Marks all active slaves as synced

### Automatic Sync Triggers

**1. New Slave Connection:**
- Called every 2 seconds in master loop
- Checks `trackedSlaves[i].configSynced` flag
- Broadcasts if any slave needs sync

**2. Master Config Save:**
- Before reboot, broadcasts all configs
- Ensures slaves get new config immediately
- 1 second delay for all messages to send

---

## Message Details

### RoundConfigMessage Structure

```cpp
struct RoundConfigMessage {
  uint8_t messageType;      // MSG_ROUND_CONFIG
  uint8_t roundIndex;       // Which round (0-24)
  uint16_t duration;        // Minutes
  uint16_t smallBlind;
  uint16_t bigBlind;
  uint16_t ante;
  bool isBreak;
  uint8_t totalRounds;      // Total number of rounds
};
```

**Size:** 13 bytes per round config  
**Total:** 325 bytes for all 25 rounds  
**Time:** ~500ms to send all configs (20ms × 25)

---

## Serial Monitor Output

### Master Logs

```
New slave connected: 11:22:33:44:55:66 (Total slaves: 1)
Broadcasting all 25 round configs to slaves...
All round configs broadcasted
```

### When Config Changes

```
Master config changed - broadcasting to slaves...
Broadcasting all 25 round configs to slaves...
All round configs broadcasted
```

### Slave Logs

```
Slave received config for round 1
Slave received config for round 2
Slave received config for round 3
...
Slave received config for round 25
```

---

## Testing Checklist

### Basic Sync Test

- [ ] Start master with custom blind structure
- [ ] Start slave (can have different config)
- [ ] Wait 5 seconds
- [ ] Check slave serial: Should show "Slave received config for round 1-25"
- [ ] Advance master to round 5
- [ ] Check slave display matches master's blinds/antes

### Config Change Test

- [ ] Master and slave both running
- [ ] Change master config via web interface
- [ ] Save on master
- [ ] Check master serial: "Broadcasting all 25 round configs"
- [ ] Slave should receive all new configs
- [ ] After master reboots, slave should show new blinds

### Multiple Slaves Test

- [ ] Start master
- [ ] Start 3 slaves with different configs
- [ ] All should receive full config sync
- [ ] All should display identical blinds/antes
- [ ] Change master config
- [ ] All 3 slaves should receive updates

---

## Comparison: Before vs After

### BEFORE (Broken)

```
Master Config:
  Round 1: 100/200
  Round 2: 200/400
  Round 3: 300/600

Slave Config (different):
  Round 1: 25/50
  Round 2: 50/100
  Round 3: 75/150

Master displays: Round 2 - 200/400 ✓
Slave displays:  Round 2 - 50/100  ❌ WRONG!
```

### AFTER (Fixed)

```
Master Config:
  Round 1: 100/200
  Round 2: 200/400
  Round 3: 300/600

Slave receives master's config automatically...

Master displays: Round 2 - 200/400 ✓
Slave displays:  Round 2 - 200/400 ✓ CORRECT!
```

---

## Code Changes Summary

| File | Lines | Function | Change |
|------|-------|----------|--------|
| main.cpp:100-104 | SlaveInfo struct | Added `configSynced` flag |
| main.cpp:175-177 | Prototypes | Added `broadcastAllRoundConfigs()` and `syncSlaveConfigs()` |
| main.cpp:1489 | updateSlaveTracking() | Set `configSynced = false` for new slaves |
| main.cpp:1646-1657 | broadcastAllRoundConfigs() | NEW: Broadcast all round configs |
| main.cpp:1659-1680 | syncSlaveConfigs() | NEW: Check and sync slave configs |
| main.cpp:1904 | setup() | Initialize `configSynced = false` |
| main.cpp:898-906 | /save handler | Broadcast configs before reboot |
| main.cpp:2078-2083 | loop() | Call syncSlaveConfigs() every 2 seconds |

---

## Benefits

✅ **Automatic sync** - No manual configuration needed  
✅ **Always correct** - Slaves always show master's blinds  
✅ **Fast sync** - Under 1 second for all configs  
✅ **Handles config changes** - Updates pushed automatically  
✅ **Works with multiple slaves** - All get same config  
✅ **Resilient** - Re-syncs if slave reconnects  

---

## Troubleshooting

### Slave Shows Wrong Blinds

**Check:**
1. Master serial shows "Broadcasting all 25 round configs"?
2. Slave serial shows "Slave received config for round X"?
3. Wait 5 seconds after slave connects

**Fix:**
- Restart both devices
- Check master is in Master mode
- Verify slave receives heartbeat acknowledgment

### Slave Not Receiving Configs

**Check:**
1. Master shows correct slave count (M:1S)?
2. Slave shows green S (connected)?
3. Serial monitor for both devices

**Fix:**
- Check ESPNow initialized on both
- Verify both devices within range
- Check message size (should be 13 bytes)

### Config Changes Not Updating Slaves

**Check:**
1. Did master broadcast before reboot?
2. Serial shows "Broadcasting all 25 round configs"?
3. Slaves were connected when config saved?

**Fix:**
- Restart slaves to trigger re-sync
- Check slave serial for "Slave received config" messages
- Verify master in Master mode when saving

---

**All changes implemented - ready to test!**

Master now automatically ensures all slaves display the correct blinds, antes, and breaks.
