# Multi-Game Configuration Feature

**Version:** 3.0  
**Date:** June 12, 2026  
**Build:** 00:57:55

## Overview

The Poker Timer now supports **3 independent game configurations** with seamless switching and management. Users can configure and save three different tournament structures and easily switch between them.

## Key Features

### 1. Three Game Slots
- **Game Slot 0, 1, 2** - Each with independent configuration
- **Custom names** - Up to 31 characters per game
- **25 rounds per game** - Full blind structure for each
- **Independent settings** - Duration, blinds, antes, breaks

### 2. Game Selection Screen
**Appears when:**
- Device powers on with no saved timer state
- User presses "Fresh" button on Resume/Fresh screen

**Features:**
- **Simple interface** - Three buttons labeled "1", "2", "3"
- **Game names displayed** - Shows configured name next to each button
- **Bottom text** - "Select Game" centered at bottom
- **One-touch selection** - Tap button to load that game

**Layout:**
```
┌─────────────────────────────────────┐
│ [1]  ANAF302 Sidney                │
│                                     │
│                                     │
│ [2]  20 Minute Rounds              │
│                                     │
│                                     │
│ [3]  30 Minute Rounds              │
│                                     │
│                                     │
│                                     │
│           Select Game               │
└─────────────────────────────────────┘
```

### 3. Web Configuration Interface

**Game Selector:**
- **Radio buttons** at top: ⭘ 1  ⭘ 2  ⭘ 3
- **Game Name field** - Edit game title
- **Seamless switching** - Change between games without losing edits
- **Client-side storage** - All 3 games held in browser memory
- **Single save** - All changes saved at once on submit

**Workflow:**
1. Open config page → All 3 games pre-loaded
2. Edit Game 1 → Switch to Game 2 (changes preserved)
3. Edit Game 2 → Switch to Game 3 (changes preserved)
4. Click Save → All 3 games written to device
5. Device reboots → All changes applied

**Benefits:**
- ✅ No data loss when switching games
- ✅ No confirmation warnings
- ✅ No multiple reboots needed
- ✅ Edit all games in one session

### 4. Import/Export

**Export:**
- Exports **all 3 games** in single JSON file
- Filename: `poker_3games_YYYY-MM-DD.json`
- Format version: 3.0
- Includes all game names and round configurations

**Import:**
- Supports **new multi-game format** (v3.0)
- **Backward compatible** with old single-game format (v2.0)
- Old format → Imports to current game slot
- New format → Loads all 3 games

**JSON Format:**
```json
{
  "version": "3.0",
  "exportDate": "2026-06-12T01:23:45.678Z",
  "deviceMode": "0",
  "games": [
    {
      "name": "ANAF302 Sidney",
      "rounds": [...]
    },
    {
      "name": "20 Minute Rounds",
      "rounds": [...]
    },
    {
      "name": "30 Minute Rounds",
      "rounds": [...]
    }
  ]
}
```

### 5. Smart State Saving

**New Behavior:**
- Saved state **only created** when START button pressed
- If user never starts timer → No saved state
- Next power-on → Game selection screen shown
- Once started → State saves every 10 seconds (as before)

**Flag:** `hasBeenStarted`
- Set to `true` when START pressed first time
- Reset to `false` when Fresh pressed or new game selected
- State only saves if flag is `true`

**Scenarios:**

| Action | hasBeenStarted | State Saved? | Next Boot |
|--------|---------------|--------------|-----------|
| Fresh → Select game → Never start | false | No | Game selection |
| Fresh → Select game → Press START | true | Yes (every 10s) | Resume/Fresh |
| Resume → Continue playing | true | Yes (every 10s) | Resume/Fresh |
| Fresh → Resets flag | false | No | Game selection |

## Storage

### NVS Memory Layout

**Device Settings:**
- `deviceMode` - Master/Slave/Standalone
- `selectedGame` - Currently selected game slot (0-2)

**Per Game (g0, g1, g2):**
- `g0_name` - Game name (string, max 31 chars)
- `g0_total` - Total rounds for this game
- `g0_r0_dur` - Round 0 duration
- `g0_r0_sb` - Round 0 small blind
- `g0_r0_bb` - Round 0 big blind
- `g0_r0_ante` - Round 0 ante
- `g0_r0_brk` - Round 0 is break (boolean)
- ... (repeated for all 25 rounds)

**Timer State:**
- `state_round` - Current round number
- `state_seconds` - Remaining seconds
- `state_running` - Timer running flag
- `state_time` - Timestamp

### Memory Usage

**Per Game:**
- Game name: 32 bytes
- Rounds: 25 rounds × 5 values × 4 bytes = 500 bytes
- Total rounds: 4 bytes
- **Subtotal: ~536 bytes per game**

**Total Storage:**
- 3 games: ~1,608 bytes
- Device settings: ~16 bytes
- Timer state: ~16 bytes
- **Total: ~1,640 bytes**

**Available:** 20KB NVS partition = **Only 8% used!**

### Data Migration

**Automatic migration** from old single-game format:
- Old config found → Migrates to Game Slot 0
- Games 1 and 2 → Initialized with defaults
- No data loss
- Seamless upgrade

## Technical Implementation

### Key Components

**1. Data Structures:**
```cpp
#define NUM_GAME_SLOTS 3
#define MAX_GAME_NAME_LENGTH 32
#define MAX_ROUNDS 25

struct GameConfig {
  char gameName[MAX_GAME_NAME_LENGTH];
  Round rounds[MAX_ROUNDS];
  int totalRounds;
};

GameConfig gameConfigs[NUM_GAME_SLOTS];
int selectedGameSlot = 0;
bool hasBeenStarted = false;
```

**2. Key Functions:**
- `initializeDefaultGame(slot, name)` - Initialize game with defaults
- `loadGameIntoActive(slot)` - Load game into active arrays
- `saveActiveToGame(slot)` - Save active game to slot
- `showGameSelectionScreen()` - Display game picker UI
- `saveTimerState()` - Only saves if hasBeenStarted == true
- `clearTimerState()` - Clears state and resets hasBeenStarted flag

**3. Web Interface:**
- JavaScript client-side storage for all 3 games
- `gamesData` object holds all games in browser memory
- `saveCurrentGameToMemory()` - Preserves changes when switching
- `loadGameFromMemory(slot)` - Loads game into form
- `switchGame()` - Seamless game switching without page reload

**4. Server Handler:**
- Uses ArduinoJson library for reliable parsing
- Handles chunked HTTP bodies (large JSON files)
- Accumulates all chunks before parsing
- Validates and saves all 3 games atomically
- `preferences.clear()` removes old data before saving

### Bug Fixes Applied

**1. WiFi AP Startup Issue:**
- Added WiFi hardware initialization before AP mode
- `WiFi.begin()` to wake up radio
- Multiple retry attempts with progressive simplification
- Now starts successfully from STANDALONE mode

**2. Chunked HTTP Body Handling:**
- Large JSON split into multiple packets by browser
- Added chunk accumulation logic
- Only parse when all chunks received
- Increased buffer to 12KB for large configs

**3. Stale NVS Data:**
- Old game names persisting across saves
- Added `preferences.clear()` before saving
- Ensures clean slate for new data
- Verification check after save confirms write

**4. Saved State Logic:**
- State was saving even when timer never started
- Added `hasBeenStarted` flag
- State only persists after START pressed
- Enables proper game selection on fresh boot

## User Interface Updates

### Removed Elements
- ❌ Device Sync Mode from web config (now set via device screen only)
- ❌ "Starting fresh..." intermediate screen
- ❌ Confirmation warnings when switching games

### Updated Elements
- ✅ Game selector: Dropdown → Radio buttons (1, 2, 3)
- ✅ "Loading: [name]" text: Size 2 → Size 1 (prevents wrapping)
- ✅ "Select Game" title: Top → Bottom center
- ✅ Game buttons: "GAME1" → "1" (cleaner labels)

## Configuration Mode Changes

### Access Point
- Properly de-initializes ESP-NOW before starting AP
- Initializes WiFi hardware if needed
- Multiple retry strategies for reliable startup
- Enhanced debug logging

### Web Server
- Supports query parameter: `?game=X` for direct game loading
- Saves current game before switching
- All 3 games sent on submit
- Device mode preserved from C++ variable (not form)

### JSON Processing
- ArduinoJson library for reliable parsing
- 12KB buffer for large configurations
- Chunk accumulation for multi-packet transfers
- Comprehensive error handling and validation

## Testing Checklist

### Game Selection
- [ ] Power on fresh → Game selection screen shown
- [ ] Select each game (1, 2, 3) → Loads correctly
- [ ] Game names display next to buttons
- [ ] "Select Game" text at bottom center

### Web Configuration
- [ ] Radio buttons (1, 2, 3) switch games instantly
- [ ] Edit Game 1 → Switch to Game 2 → Changes preserved
- [ ] Edit all 3 games → Save → All changes applied
- [ ] Game names save and display correctly
- [ ] Single reboot after saving all changes

### State Saving
- [ ] Select game, don't start → Power off/on → Game selection screen
- [ ] Select game, press START → Power off/on → Resume/Fresh screen
- [ ] Resume → Timer state continues from where left off
- [ ] Fresh → Game selection → Select different game

### Import/Export
- [ ] Export → Downloads all 3 games in single file
- [ ] Import new format → All 3 games loaded
- [ ] Import old format → Loads to current game slot
- [ ] Exported configs work across devices

### WiFi & Config Mode
- [ ] Config mode starts from STANDALONE mode
- [ ] Config mode starts from Master/Slave mode
- [ ] Large configs save without errors
- [ ] Device reboots after save
- [ ] All games load correctly after reboot

## Migration Guide

### From Version 2.x → 3.0

**Automatic Migration:**
1. Upload new firmware
2. Device boots → Loads old config into Game Slot 0
3. Games 1 and 2 → Initialized with defaults
4. No manual intervention required

**Recommended Steps:**
1. **Before upgrade:** Export current config (backup)
2. **Upload firmware:** Flash version 3.0
3. **Verify:** Open config page, check Game 1 has old settings
4. **Configure:** Set up Games 2 and 3 as needed
5. **Test:** Try each game to confirm rounds are correct
6. **Export:** Create new multi-game backup

### Rollback (if needed)
1. Flash old firmware (2.x)
2. Device will load Game Slot 0 as single game
3. Other games ignored (but data preserved in NVS)
4. Can upgrade again without data loss

## Troubleshooting

### Game Selection Screen Not Appearing
- **Issue:** Shows Resume/Fresh screen instead
- **Cause:** Saved state exists from previous session
- **Fix:** Press Fresh button → Clears state → Next boot shows game selection

### Config Not Saving
- **Issue:** Changes don't persist after save/reboot
- **Cause:** Large JSON not fully received or parsing error
- **Fix:** Check Serial Monitor for parse errors. Try configuring fewer rounds.

### WiFi AP Won't Start
- **Issue:** "AP FAILED!" on screen
- **Cause:** WiFi hardware not initialized or ESP-NOW conflict
- **Fix:** Already fixed in v3.0 - ensure you're on latest build

### Game Names Not Saving
- **Issue:** Shows "Game 2" instead of custom name
- **Cause:** Stale NVS data or save didn't complete
- **Fix:** Already fixed in v3.0 (preferences.clear() before save)

### State Not Resuming
- **Issue:** Always shows game selection, even after starting timer
- **Cause:** hasBeenStarted flag not set properly
- **Fix:** Already fixed in v3.0 - flag set when START pressed and when Resume pressed

## Version History

**v3.0 (June 12, 2026):**
- ✨ Added 3 game configuration slots
- ✨ Game selection screen on startup
- ✨ Multi-game web configuration interface
- ✨ Enhanced import/export (all games)
- ✨ Smart state saving (only after START)
- 🐛 Fixed WiFi AP startup from STANDALONE mode
- 🐛 Fixed chunked HTTP body handling
- 🐛 Fixed stale NVS data persistence
- 🐛 Fixed state saving logic
- 🎨 Updated UI (radio buttons, smaller fonts)
- 🎨 Removed device mode from web config

**v2.x:**
- Single game configuration
- Basic import/export
- State saved every 10 seconds (always)

## Future Enhancements

**Potential additions (not yet implemented):**
- [ ] Game templates library (Turbo, Standard, Deep Stack, etc.)
- [ ] Quick game duplication (copy Game 1 → Game 2)
- [ ] Game comparison view (side-by-side)
- [ ] Increase to 5 or 10 game slots (plenty of memory available)
- [ ] Game statistics (times played, average duration)
- [ ] Cloud backup/sync (Google Drive, Dropbox)
- [ ] QR code sharing (export to QR, scan to import)
- [ ] Game scheduling (auto-switch at specific times)

## Credits

**Development:** OpenCode AI Assistant  
**Hardware:** ESP32-2432S028 (CYD)  
**Libraries:** TFT_eSPI, ArduinoJson, ESPAsyncWebServer  
**Testing:** Rob @ RFLTOOLS

---

**Enjoy your multi-game poker timer!** 🎰🃏
