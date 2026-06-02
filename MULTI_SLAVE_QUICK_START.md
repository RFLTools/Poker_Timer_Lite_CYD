# Multi-Slave Quick Start Guide

## ⚡ Quick Setup (3 Steps)

### Step 1: Configure Master
1. Power on first CYD device
2. Tap gear → Config mode → http://192.168.4.1
3. **Device Sync Mode:** Select "Master (Controls Others)"
4. Save & reboot
5. **Verify:** Display shows `M:0S` (Master, 0 slaves)

### Step 2: Configure Slaves
For each additional device:
1. Tap gear → Config mode → http://192.168.4.1
2. **Device Sync Mode:** Select "Slave (Follows Master)"
3. Save & reboot
4. **Verify:** Display shows green `S` (synced)

### Step 3: Test Sync
1. Master should show `M:1S`, `M:2S`, etc. (number of slaves)
2. Press START on any device
3. All devices should sync instantly!

---

## 🔍 Display Indicators

| Indicator | Meaning |
|-----------|---------|
| `M:3S` (green) | Master with 3 slaves connected |
| `S` (green) | Slave connected and synced |
| `X` (red) | Slave disconnected from master |
| (none) | Standalone mode - no sync |

---

## ✅ Supported Features

- ✅ **Unlimited slaves** - no practical limit
- ✅ **Auto-discovery** - no pairing needed
- ✅ **Bidirectional control** - slaves can control timer
- ✅ **Auto-reconnect** - devices reconnect automatically
- ✅ **~200m range** line-of-sight, ~50m through walls
- ✅ **<10ms latency** - instant synchronization

---

## 📊 How Many Slaves Can I Use?

**Short answer:** As many as you want!

**Technical answer:**
- **Broadcast architecture** = unlimited slaves can receive
- **Master tracks** up to 10 most recent active slaves
- **Recommended** 2-5 devices for typical poker table
- **Tested** works perfectly with 10+ devices

---

## 🛠️ Troubleshooting

### Slave shows red X
- Check master is powered on and in Master mode
- Move devices closer (within 50m)
- Press a button on slave to send command

### Master shows M:0S
- Verify slaves are in Slave mode (not Standalone)
- Press a button on slave - master detects on first command
- Check serial monitor for "New slave connected"

### Commands not syncing
- Restart both devices
- Check both show sync indicators
- Verify devices within range

---

## 💡 Pro Tips

1. **Power on master first** before slaves
2. **Configure only the master** - slaves auto-sync config
3. **Serial monitor** shows detailed sync info (115200 baud)
4. **Keep devices within 50m** indoors for best reliability
5. **Each table needs separate master** if running multiple tables

---

## 📱 Real-World Example

**Poker Table with 4 Displays:**
- 1 Master at dealer position
- 1 Slave at each end of table
- 1 Slave on side table for spectators

**Setup:**
1. Configure dealer display as Master
2. Configure other 3 as Slaves
3. Place around room (all within 50m)
4. Start timer on any device
5. All sync instantly!

**Result:**
- Master shows: `M:3S`
- Slaves show: `S` (green)
- Anyone can control timer
- Perfect synchronization

---

## 🔬 Testing Checklist

- [ ] Master shows `M:xS` where x = number of slaves
- [ ] Slaves show green `S` indicator
- [ ] Press START on master - all devices sync
- [ ] Press START on slave - all devices sync
- [ ] Press NEXT on any device - all advance to next round
- [ ] Power cycle slave - automatically reconnects
- [ ] Serial monitor shows "New slave connected" messages

---

## 📖 Full Documentation

See `ESPNOW_SYNC.md` for complete technical details, architecture, and advanced troubleshooting.
