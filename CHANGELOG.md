# Changelog

All notable changes to the Poker Tournament Timer project will be documented in this file.

## [3.0.0] - 2026-06-12

### 🎮 Added - Multi-Game Configuration
- **3 Game Slots** - Store and manage 3 independent tournament configurations
- **Game Selection Screen** - Appears on fresh boot, allows choosing which game to start
- **Custom Game Names** - Each game can have a custom name (up to 31 characters)
- **Game Picker UI** - Simple 1/2/3 button interface with game names displayed
- **Smart State Saving** - Only saves timer state after START button pressed
  - If never started → Game selection screen on next boot
  - If started → Resume/Fresh screen on next boot

### 🌐 Enhanced - Web Configuration
- **Radio Button Selector** - Replaced dropdown with cleaner 1/2/3 radio buttons
- **Seamless Multi-Game Editing** - Edit all 3 games in one session
- **Client-Side Storage** - All games held in browser memory
- **No Data Loss** - Switch between games without losing changes
- **Single Save Operation** - All 3 games saved at once (one reboot)
- **Removed Device Sync Mode** - Now configured via device screen only

### 💾 Enhanced - Import/Export
- **Export All Games** - Single JSON file contains all 3 game configurations
- **Version 3.0 Format** - New multi-game JSON structure
- **Backward Compatible** - Still imports old single-game format (v2.0)
- **Improved Filenames** - `poker_3games_YYYY-MM-DD.json`

### 🐛 Fixed - WiFi & Networking
- **AP Startup from STANDALONE** - Fixed WiFi initialization when ESP-NOW not running
- **WiFi Hardware Init** - Added `WiFi.begin()` to properly wake up radio
- **Multiple Retry Strategies** - Progressive simplification if first attempt fails
- **Enhanced Debug Logging** - Better visibility into AP startup process

### 🐛 Fixed - Web Server
- **Chunked HTTP Bodies** - Properly handle large JSON split across multiple packets
- **Request Accumulation** - Wait for all chunks before parsing
- **Increased Buffer Size** - 12KB JSON buffer for large configurations
- **ArduinoJson Integration** - Reliable JSON parsing instead of manual string manipulation

### 🐛 Fixed - Data Persistence
- **Stale NVS Data** - Added `preferences.clear()` before saving to remove old keys
- **Save Verification** - Read-back check after save confirms write success
- **Game Name Persistence** - Fixed issue where custom names reverted to defaults
- **Atomic Save Operation** - All games saved in single transaction

### 🐛 Fixed - State Management
- **hasBeenStarted Flag** - New flag tracks if timer has been started
- **Conditional Saving** - State only persists after START pressed
- **Resume Flag Setting** - Flag properly set when resuming saved state
- **Fresh Button Behavior** - Clears state and resets flag

### 🎨 Improved - User Interface
- **Smaller Loading Font** - Size 1 instead of 2, prevents text wrapping
- **Repositioned Title** - "Select Game" moved from top to bottom center
- **Simplified Button Labels** - "1", "2", "3" instead of "GAME1", "GAME2", "GAME3"
- **Removed Intermediate Screen** - No "Starting fresh..." delay
- **No Confirmation Warnings** - Seamless game switching in web UI

### 📊 Technical Improvements
- **Memory Efficient** - Only 8% of NVS partition used (~1.6KB of 20KB)
- **Automatic Migration** - Old single-game configs migrate to Game Slot 0
- **Better Error Handling** - Comprehensive validation and error messages
- **Enhanced Logging** - Detailed Serial Monitor output for debugging

### 📝 Documentation
- **MULTI_GAME_FEATURE.md** - Comprehensive multi-game feature documentation
- **Updated README.md** - Added multi-game feature highlights
- **Migration Guide** - Instructions for upgrading from v2.x
- **Testing Checklist** - Complete validation procedures
- **Troubleshooting** - Common issues and solutions

---

## [2.x.x] - Previous Versions

### Features
- Single game configuration (25 rounds)
- Web-based configuration interface
- Import/Export single game
- State saved every 10 seconds (always)
- Master/Slave synchronization via ESP-NOW
- Touch-based controls
- Audio alerts

### Issues (Fixed in 3.0)
- No multi-game support
- State saved even when never started
- Config switching required page reload (lost changes)
- WiFi AP startup issues from STANDALONE mode
- Stale NVS data could persist

---

## Version Numbering

This project uses [Semantic Versioning](https://semver.org/):
- **MAJOR** version for incompatible changes
- **MINOR** version for backwards-compatible functionality additions
- **PATCH** version for backwards-compatible bug fixes

**v3.0.0** is a MAJOR version because:
- New storage format (NVS key structure changed)
- New JSON export format (v3.0)
- New web UI structure (radio buttons vs dropdown)
- Behavioral changes (state saving logic)
- Automatic migration included for seamless upgrade

---

## Upgrade Notes

### From 2.x → 3.0

**✅ Automatic Migration:**
- Your existing config will automatically migrate to Game Slot 0
- Games 1 and 2 will be initialized with defaults
- No manual intervention required

**⚠️ Breaking Changes:**
- Export files from v2.x will import as single game (to current slot)
- Old export format not compatible with multi-game import
- State saving behavior changed (only after START pressed)

**📋 Recommended Steps:**
1. Export your current config (backup)
2. Upload v3.0 firmware
3. Verify Game 1 has your old settings
4. Configure Games 2 and 3 as needed
5. Export new multi-game backup

### Rollback Procedure

If you need to revert to v2.x:
1. Flash v2.x firmware
2. Device will use Game Slot 0 data
3. Other games ignored but preserved in NVS
4. Can upgrade again later without data loss

---

## Future Roadmap

**Under Consideration:**
- [ ] Increase to 5 or 10 game slots
- [ ] Game templates library (Turbo, Deep Stack, etc.)
- [ ] Quick game duplication feature
- [ ] Cloud backup/sync integration
- [ ] QR code sharing
- [ ] Game statistics tracking

**Community Requests Welcome:**
Submit feature requests via GitHub issues!

---

## Credits

**Development Team:**
- OpenCode AI Assistant - Feature development
- Rob @ RFLTOOLS - Testing and validation

**Libraries:**
- TFT_eSPI by Bodmer
- ArduinoJson by Benoit Blanchon
- ESPAsyncWebServer by ESPHome
- XPT2046_Touchscreen by Paul Stoffregen

**Hardware:**
- ESP32-2432S028 (CYD - Cheap Yellow Display)

---

**Last Updated:** June 12, 2026
