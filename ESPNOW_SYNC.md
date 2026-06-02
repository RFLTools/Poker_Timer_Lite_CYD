# ESPNow Multi-Device Synchronization

## Overview

The Poker Timer now supports **unlimited slave devices** synchronizing with a single master device using ESP-NOW wireless protocol. This allows you to have multiple displays around your poker table, all showing the same timer in perfect sync.

## Architecture

### Broadcast-Based Design
- **Master** broadcasts timer state to all devices in range (no limit on number of slaves)
- **Slaves** receive broadcasts and update their displays
- **Bidirectional control** - slaves can send commands back to master
- **Automatic discovery** - no pairing or configuration needed

### Device Modes

#### Standalone (Default)
- Single device operation
- No ESPNow functionality
- Works exactly as before

#### Master
- Runs the actual timer logic
- Broadcasts state every 1 second to all nearby devices
- Receives and executes control commands from slaves
- Tracks connected slaves and displays count
- Can have **unlimited slaves** connected simultaneously

#### Slave
- Displays timer state received from master
- Sends control commands when buttons are pressed
- Shows connection status (green=connected, red=disconnected)
- Config button disabled (config only on master)

## Features

### Multi-Slave Support (Unlimited)
✅ **No limit on slave count** - broadcast architecture supports unlimited slaves  
✅ **Automatic slave tracking** - master tracks up to 10 most recent slaves  
✅ **Slave count display** - master shows "M:3S" (3 slaves connected)  
✅ **Connection monitoring** - inactive slaves cleaned up after 10 seconds  
✅ **MAC address logging** - master logs which slave sent each command  

### Synchronization
✅ **1Hz updates** - timer state broadcast every second  
✅ **Low latency** - typically <10ms ESPNow delay  
✅ **Full state sync** - round, time, blinds, antes, running state  
✅ **Configuration sync** - master can push round config to slaves  
✅ **Auto-reconnect** - slaves auto-reconnect if master restarts  

### Display Indicators

**Master Device:**
- Top-left shows: `M:3S` (Master with 3 slaves connected)
- Green text = ESPNow active

**Slave Device:**
- Top-left shows: `S` (green) = Connected and synced
- Top-left shows: `X` (red) = Disconnected from master
- No config button (config disabled on slaves)

## Setup Instructions

### 1. Configure Master Device

1. Power on your primary CYD device
2. Tap the **gear icon** (top-right)
3. Confirm to enter config mode
4. Connect to WiFi: **PokerTimer**
5. Open browser: **http://192.168.4.1**
6. Under "Device Sync Mode", select **Master (Controls Others)**
7. Configure your tournament rounds as needed
8. Click **Save** - device reboots
9. Verify "M:0S" appears on display (0 slaves initially)

### 2. Configure Slave Devices

Repeat for each slave device:

1. Power on the CYD device
2. Tap the **gear icon**
3. Confirm to enter config mode
4. Connect to WiFi: **PokerTimer**
5. Open browser: **http://192.168.4.1**
6. Under "Device Sync Mode", select **Slave (Follows Master)**
7. Click **Save** - device reboots
8. Verify green "S" appears on display (synced)

### 3. Verify Synchronization

1. **On Master:** Display should show `M:1S`, `M:2S`, etc. as slaves connect
2. **On Slaves:** Display should show green `S` indicator
3. **Press START on any device** - all devices should sync
4. **Skip to next round on any device** - all devices should update

## Technical Details

### Broadcast Architecture
- **Broadcast MAC:** FF:FF:FF:FF:FF:FF (all devices)
- **Master→Slaves:** Timer state every 1 second
- **Slave→Master:** Control commands on button press
- **No pairing required:** Devices automatically find each other

### Slave Tracking
- Master tracks up to **10 most recent slaves**
- Slaves tracked by MAC address
- 10-second inactivity timeout
- Cleanup runs every 5 seconds
- Serial monitor shows slave connect/disconnect events

### Message Protocol

**Timer State Broadcast (8 bytes):**
```
Type: MSG_TIMER_STATE
Data: currentRound, remainingSeconds, timerRunning, timestamp
Frequency: 1 Hz (every second)
```

**Control Command (3 bytes):**
```
Type: MSG_CONTROL_CMD
Data: command (START_PAUSE/NEXT/PREV), slaveId
Sent: On button press from slave
```

**Round Config (13 bytes):**
```
Type: MSG_ROUND_CONFIG
Data: roundIndex, duration, smallBlind, bigBlind, ante, isBreak, totalRounds
Sent: When master config changes or on slave sync request
```

### Connection Monitoring

**Master:**
- Considers slave "active" if command received within 10 seconds
- Shows active slave count on display
- Logs slave MAC addresses in serial monitor

**Slave:**
- Considers master "connected" if broadcast received within 5 seconds
- Shows green "S" when connected
- Shows red "X" when disconnected
- Automatically reconnects when master comes back online

## Range and Performance

### Range
- **Line of Sight:** ~200 meters (650 feet)
- **Through Walls:** ~50 meters (165 feet) typical
- **Same Room:** Excellent reliability
- **Adjacent Rooms:** Very good reliability

### Performance
- **Latency:** <10ms typical ESPNow delay
- **Update Rate:** 1 Hz (master broadcasts every second)
- **Bandwidth:** ~8 bytes/second per connection
- **Power:** Low power consumption (ESPNow is very efficient)

### Scalability
- **Theoretical Limit:** 250+ devices (ESPNow spec)
- **Practical Limit:** Unlimited slaves (broadcast architecture)
- **Tracked Slaves:** Master tracks 10 most recent (display shows count)
- **Recommended:** 2-5 devices for poker table use

## Troubleshooting

### Slave Shows Red "X" (Disconnected)

**Check:**
1. Master device is powered on and in Master mode
2. Devices are within range (~50m indoors)
3. Master's serial monitor shows broadcasts happening
4. Slave's serial monitor shows "Slave requesting full sync"

**Fix:**
- Move devices closer together
- Verify master is in Master mode (check config)
- Restart both devices
- Check for WiFi interference (ESPNow uses WiFi frequencies)

### Master Shows "M:0S" (No Slaves)

**Possible Causes:**
1. Slave devices not configured as slaves
2. Slaves not powered on or restarting
3. Slaves not pressing any buttons (tracking requires activity)

**Fix:**
- Verify slave devices are in Slave mode (check config)
- Press a button on slave device to send a command
- Master will detect and track the slave
- Check master's serial monitor for "New slave connected" messages

### Commands Not Syncing

**Check:**
1. Both devices show sync indicators (M:xS and S)
2. Serial monitors show message sending/receiving
3. No WiFi interference

**Fix:**
- Restart both devices
- Move devices closer during testing
- Verify ESPNow initialized (check serial monitor on startup)

### Multiple Slaves Sending Commands Simultaneously

**ESPNow handles this automatically:**
- Built-in collision avoidance at protocol level
- Commands are queued and processed sequentially
- Master processes commands in order received
- Very rare for collisions in real-world use

## Serial Monitor Logging

Enable Serial Monitor (115200 baud) to see detailed sync information:

### Master Logs:
```
✓ ESPNow initialized successfully
✓ Broadcast peer added (Master mode)
Device mode: MASTER
New slave connected: AA:BB:CC:DD:EE:FF (Total slaves: 1)
Master received command from slave AA:BB:CC:DD:EE:FF: 1
```

### Slave Logs:
```
✓ ESPNow initialized successfully
Device mode: SLAVE
Slave requesting full sync from master...
Slave sent command: 1
```

## Best Practices

### For Optimal Performance:

1. **Power on master first** before slaves
2. **Keep devices within 50m** indoors for reliable sync
3. **Use quality power supplies** - brownouts can cause disconnects
4. **Monitor slave count** on master display
5. **Check serial monitor** during setup to verify sync

### Configuration Changes:

1. **Only configure master** - slaves follow master's config
2. **Reconfigure master** if you need to change tournament structure
3. **Slaves auto-sync** when master config changes
4. **Mode changes** require device reboot (automatic after save)

### Multiple Table Setup:

If you have multiple poker tables:

1. **Use separate masters** for each table
2. **Keep tables 50m+ apart** to avoid cross-talk (or same room is fine)
3. **ESPNow is peer-aware** - slaves will only follow their master
4. **Broadcast is localized** - minimal interference between tables

## Hardware Requirements

### All Devices:
- ESP32-2432S028 (CYD - Cheap Yellow Display)
- 240x320 ILI9341 TFT Display
- XPT2046 Resistive Touch Controller
- ESP32 with WiFi (required for ESPNow)

### No Additional Hardware Needed:
- ESPNow uses built-in ESP32 WiFi hardware
- No external radio modules required
- No additional antennas needed
- Works with standard CYD configuration

## Code Reference

### Key Files:
- `src/main.cpp` - All ESPNow implementation

### Key Functions:
- `initESPNow()` - Initialize ESPNow protocol (line ~1268)
- `broadcastTimerState()` - Master broadcasts state (line ~1342)
- `handleControlCommandMessage()` - Master processes slave commands (line ~1416)
- `updateSlaveTracking()` - Track active slaves (line ~1228)
- `getActiveSlaveCount()` - Get connected slave count (line ~1263)

### Configuration:
- Device mode stored in NVS/Preferences
- Persists across reboots
- Configurable via web interface

## License

Same as main project: NON-COMMERCIAL USE ONLY

---

**Need Help?**

Check the serial monitor logs at 115200 baud for detailed diagnostics.
Post issues to the project repository with:
- Device modes (master/slave)
- Serial monitor output
- Number of devices
- Distance between devices
