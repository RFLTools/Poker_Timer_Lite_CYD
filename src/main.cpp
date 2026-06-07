/*
 * Poker Tournament Blind Timer
 * ESP32 CYD (Cheap Yellow Display) with ILI9341 Display and XPT2046 Touch
 * 
 * Copyright (c) 2026 Robert Livingston
 * Licensed for NON-COMMERCIAL use only
 * 
 * This software may NOT be used for commercial purposes without permission.
 * See LICENSE file for full terms.
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Touch screen configuration - CYD uses SEPARATE SPI bus for touch!
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25

// Create separate SPI instance for touch (CYD specific)
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Display setup
TFT_eSPI tft = TFT_eSPI();

// Display dimensions: 320x240 (LANDSCAPE)
// Using rotation=1 for landscape orientation
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

// Backlight control
#define TFT_BL 21

// Buzzer control (P4 speaker connector)
#define BUZZER_PIN 26

// ESPNow configuration
enum DeviceMode {
  MODE_STANDALONE = 0,
  MODE_MASTER = 1,
  MODE_SLAVE = 2
};

// ESPNow message types
enum MessageType {
  MSG_TIMER_STATE = 1,      // Master broadcasts current timer state
  MSG_ROUND_CONFIG = 2,     // Master broadcasts round configuration
  MSG_HEARTBEAT = 3,        // Slave sends heartbeat to master
  MSG_BEEP = 4              // Master tells slaves to beep
};

// Heartbeat message (sent by slave every 5 seconds)
struct HeartbeatMessage {
  uint8_t messageType;      // MSG_HEARTBEAT
  uint8_t slaveId;          // For future use
};

// ESPNow timer state message (sent by master every second)
struct TimerStateMessage {
  uint8_t messageType;      // MSG_TIMER_STATE
  uint8_t currentRound;
  uint16_t remainingSeconds;
  bool timerRunning;
  uint32_t timestamp;       // For sync verification
};

// ESPNow round configuration message (sent when config changes)
struct RoundConfigMessage {
  uint8_t messageType;      // MSG_ROUND_CONFIG
  uint8_t roundIndex;       // Which round this configures
  uint16_t duration;
  uint16_t smallBlind;
  uint16_t bigBlind;
  uint16_t ante;
  bool isBreak;
  uint8_t totalRounds;      // Total number of rounds
};

// ESPNow beep command message (sent by master to trigger beeps on slaves)
struct BeepMessage {
  uint8_t messageType;      // MSG_BEEP
  uint8_t beepType;         // 0=button, 1=countdown warning, 2=start/stop
};

// ESPNow state
DeviceMode deviceMode = MODE_STANDALONE;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast to all
bool espNowInitialized = false;
unsigned long lastSyncReceived = 0;
const unsigned long SYNC_TIMEOUT = 5000;  // 5 seconds without sync = disconnected
bool slaveShowNextRound = true;  // For slaves: toggle next round visibility (start with next round shown)
bool isDrawing = false;  // Prevent re-entrant calls to drawTimerDisplay
bool inSettingsScreen = false;  // Prevent timer updates from overwriting settings/mode selection screen

// Slave tracking (for master to know which slaves are active)
#define MAX_TRACKED_SLAVES 10
struct SlaveInfo {
  uint8_t macAddress[6];
  unsigned long lastSeen;
  bool active;
  bool configSynced;  // Have we sent round configs to this slave?
};
SlaveInfo trackedSlaves[MAX_TRACKED_SLAVES];
int activeSlaveCount = 0;

// Preferences for storing configuration
Preferences preferences;
bool nvsAvailable = false;  // Track if NVS/Preferences is working

// Touch button structure
struct TouchButton {
  int x, y, w, h;
  String label;
  uint16_t color;
  uint16_t textColor;
};

// Define touch buttons for LANDSCAPE (320x240) - buttons on left side, stacked vertically
TouchButton btnStartPause = {10, 10, 70, 60, "START", TFT_GREEN, TFT_BLACK};  // Top left
TouchButton btnNext = {10, 80, 70, 60, "NEXT", TFT_BLUE, TFT_WHITE};          // Middle left
TouchButton btnPrev = {10, 150, 70, 60, "PREV", TFT_ORANGE, TFT_BLACK};       // Bottom left
TouchButton btnConfig = {280, 5, 30, 30, "CFG", TFT_DARKGREY, TFT_WHITE};     // Config button top right
TouchButton btnMute = {280, 205, 30, 30, "MUT", TFT_DARKGREY, TFT_WHITE};     // Mute button bottom right
TouchButton btnCancelConfig = {110, 185, 120, 50, "CANCEL", TFT_RED, TFT_WHITE}; // Cancel button for config mode

// Round structure
struct Round {
  int duration;      // Duration in minutes
  int smallBlind;
  int bigBlind;
  int ante;
  bool isBreak;
};

// Tournament settings
#define MAX_ROUNDS 25
Round rounds[MAX_ROUNDS];
int totalRounds = MAX_ROUNDS;
int currentRound = 0;
bool timerRunning = false;
unsigned long roundStartTime = 0;
int remainingSeconds = 0;
int startingSeconds = 0; // Seconds at start of current timing session

// Web server
AsyncWebServer server(80);
bool configMode = false;

// Touch debouncing
unsigned long lastTouch = 0;
const unsigned long TOUCH_DEBOUNCE = 250;

// Mute state
bool isMuted = false;

// Function prototypes
void initializeDefaultRounds();
void saveRoundsToPreferences();
void loadRoundsFromPreferences();
void saveTimerState();
void loadTimerState();
void clearTimerState();
void startConfigMode();
void drawTimerDisplay();
void drawButton(TouchButton &btn);
void handleTouch();
bool isTouchInButton(int x, int y, TouchButton &btn);
String generateConfigHTML();
void playBeep(int frequency, int duration);
void playButtonBeep();
void playRoundEndBeeps();

// ESPNow function prototypes
void initESPNow();
void onESPNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onESPNowDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
void broadcastTimerState();
void broadcastRoundConfig(int roundIndex);
void broadcastAllRoundConfigs();
void broadcastBeep(uint8_t beepType);
void syncSlaveConfigs();
void sendHeartbeat();
void handleTimerStateMessage(const TimerStateMessage *msg);
void handleRoundConfigMessage(const RoundConfigMessage *msg);
void handleHeartbeatMessage(const uint8_t *senderMac);
void handleBeepMessage(const BeepMessage *msg);
bool isSyncConnected();
void updateSlaveTracking(const uint8_t *macAddr);
void cleanupInactiveSlaves();
String macToString(const uint8_t *mac);
int getActiveSlaveCount();

void initializeDefaultRounds() {
  // Default 25 rounds, 15 minutes each
  // Break every 3 rounds (after rounds 3, 6, 9, etc.)
  
  int blinds[] = {25, 50, 75, 100, 150, 200, 300, 400, 500, 600, 
                  800, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 
                  8000, 10000, 12000, 15000, 20000, 25000, 30000};
  
  int roundIndex = 0;
  for (int i = 0; i < MAX_ROUNDS; i++) {
    // Every 4th position is a break (after 3 rounds of play)
    if ((i + 1) % 4 == 0 && i > 0) {
      rounds[i].isBreak = true;
      rounds[i].duration = 15;
      rounds[i].smallBlind = 0;
      rounds[i].bigBlind = 0;
      rounds[i].ante = 0;
    } else {
      rounds[i].isBreak = false;
      rounds[i].duration = 15;
      rounds[i].smallBlind = blinds[roundIndex];
      rounds[i].bigBlind = blinds[roundIndex] * 2;
      rounds[i].ante = 0;
      roundIndex++;
      if (roundIndex >= 25) roundIndex = 24; // Stay at max
    }
  }
}

void saveRoundsToPreferences() {
  Serial.println("\n--- Saving to Preferences ---");
  
  bool opened = preferences.begin("poker-timer", false);
  if (!opened) {
    Serial.println("✗ ERROR: Could not open preferences for writing!");
    Serial.println("Settings will NOT be saved!");
    return;
  }
  
  Serial.println("✓ Preferences opened for writing");
  
  preferences.putInt("totalRounds", totalRounds);
  preferences.putInt("deviceMode", (int)deviceMode);
  Serial.print("✓ Device mode saved: ");
  Serial.println(deviceMode == MODE_MASTER ? "MASTER" : deviceMode == MODE_SLAVE ? "SLAVE" : "STANDALONE");
  
  for (int i = 0; i < totalRounds; i++) {
    char key[20];
    sprintf(key, "r%d_dur", i);
    preferences.putInt(key, rounds[i].duration);
    sprintf(key, "r%d_sb", i);
    preferences.putInt(key, rounds[i].smallBlind);
    sprintf(key, "r%d_bb", i);
    preferences.putInt(key, rounds[i].bigBlind);
    sprintf(key, "r%d_ante", i);
    preferences.putInt(key, rounds[i].ante);
    sprintf(key, "r%d_brk", i);
    preferences.putBool(key, rounds[i].isBreak);
  }
  
  preferences.end();
  Serial.println("✓ Rounds saved to preferences successfully");
  Serial.println("-----------------------------\n");
}

void loadRoundsFromPreferences() {
  Serial.println("\n--- Loading Preferences ---");
  
  if (!nvsAvailable) {
    Serial.println("⚠ NVS not available, using defaults");
    initializeDefaultRounds();
    deviceMode = MODE_STANDALONE;
    return;
  }
  
  // Try to open preferences
  bool opened = preferences.begin("poker-timer", true);
  
  if (!opened) {
    Serial.println("✗ Failed to open preferences (this is normal on first run)");
    Serial.println("Using default configuration");
    initializeDefaultRounds();
    deviceMode = MODE_STANDALONE;
    preferences.end();
    return;
  }
  
  Serial.println("✓ Preferences opened successfully");
  
  // Load device mode
  deviceMode = (DeviceMode)preferences.getInt("deviceMode", MODE_STANDALONE);
  Serial.print("✓ Device mode loaded: ");
  Serial.println(deviceMode == MODE_MASTER ? "MASTER" : deviceMode == MODE_SLAVE ? "SLAVE" : "STANDALONE");
  
  if (preferences.isKey("totalRounds")) {
    Serial.println("✓ Found saved configuration");
    totalRounds = preferences.getInt("totalRounds", MAX_ROUNDS);
    
    for (int i = 0; i < totalRounds; i++) {
      char key[20];
      sprintf(key, "r%d_dur", i);
      rounds[i].duration = preferences.getInt(key, 15);
      sprintf(key, "r%d_sb", i);
      rounds[i].smallBlind = preferences.getInt(key, 25);
      sprintf(key, "r%d_bb", i);
      rounds[i].bigBlind = preferences.getInt(key, 50);
      sprintf(key, "r%d_ante", i);
      rounds[i].ante = preferences.getInt(key, 0);
      sprintf(key, "r%d_brk", i);
      rounds[i].isBreak = preferences.getBool(key, false);
    }
    Serial.println("✓ Rounds loaded from preferences");
  } else {
    Serial.println("No saved configuration found, using defaults");
    initializeDefaultRounds();
  }
  
  preferences.end();
  Serial.println("---------------------------\n");
}

void saveTimerState() {
  if (!nvsAvailable) return;
  
  preferences.begin("poker-timer", false);
  preferences.putInt("state_round", currentRound);
  preferences.putInt("state_seconds", remainingSeconds);
  preferences.putBool("state_running", timerRunning);
  preferences.putULong("state_time", millis());
  preferences.end();
}

void loadTimerState() {
  if (!nvsAvailable) return;
  
  preferences.begin("poker-timer", true);
  
  if (preferences.isKey("state_round")) {
    int savedRound = preferences.getInt("state_round", 0);
    int savedSeconds = preferences.getInt("state_seconds", 0);
    bool savedRunning = preferences.getBool("state_running", false);
    
    preferences.end();
    
    // Show resume option on display
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(60, 30);
    tft.println("Saved state found!");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(60, 65);
    tft.print("Round: ");
    tft.println(savedRound + 1);
    tft.setCursor(60, 85);
    tft.print("Time: ");
    tft.print(savedSeconds / 60);
    tft.print(":");
    if (savedSeconds % 60 < 10) tft.print("0");
    tft.println(savedSeconds % 60);
    
    // Draw Resume and Start Fresh buttons (landscape)
    tft.fillRoundRect(60, 130, 90, 50, 8, TFT_GREEN);
    tft.fillRoundRect(170, 130, 90, 50, 8, TFT_RED);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(67, 145);
    tft.println("RESUME");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(185, 145);
    tft.println("FRESH");
    
    // Wait for touch
    unsigned long startWait = millis();
    bool chose = false;
    while (!chose && (millis() - startWait < 10000)) {
      if (ts.touched()) {
        delay(50); // Debounce
        TS_Point p = ts.getPoint();
        // Map touch coordinates (CYD touch may need calibration)
        int x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
        int y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);
        
        // Check Resume button (left) - landscape coords
        if (x >= 60 && x <= 150 && y >= 130 && y <= 180) {
          playButtonBeep();
          // Resume
          currentRound = savedRound;
          remainingSeconds = savedSeconds;
          startingSeconds = savedSeconds;
          timerRunning = savedRunning;
          roundStartTime = millis();
          chose = true;
          
          tft.fillScreen(TFT_BLACK);
          tft.setTextSize(2);
          tft.setCursor(100, 100);
          tft.setTextColor(TFT_GREEN);
          tft.println("Resuming...");
          delay(1000);
        }
        // Check Start Fresh button (right) - landscape coords
        else if (x >= 170 && x <= 260 && y >= 130 && y <= 180) {
          playButtonBeep();
          // Start fresh
          clearTimerState();
          currentRound = 0;
          remainingSeconds = rounds[0].duration * 60;
          timerRunning = false;
          chose = true;
          
          tft.fillScreen(TFT_BLACK);
          tft.setTextSize(2);
          tft.setCursor(70, 100);
          tft.setTextColor(TFT_RED);
          tft.println("Starting fresh...");
          delay(1000);
        }
        
        // Wait for release
        while (ts.touched()) {
          delay(10);
        }
        delay(200);
      }
      delay(50);
    }
    
    // If no touch in 10 seconds, auto-resume
    if (!chose) {
      currentRound = savedRound;
      remainingSeconds = savedSeconds;
      startingSeconds = savedSeconds;
      timerRunning = savedRunning;
      roundStartTime = millis();
    }
  } else {
    preferences.end();
    // No saved state, start fresh
    currentRound = 0;
    remainingSeconds = rounds[0].duration * 60;
    timerRunning = false;
  }
}

void clearTimerState() {
  if (!nvsAvailable) return;
  
  preferences.begin("poker-timer", false);
  preferences.remove("state_round");
  preferences.remove("state_seconds");
  preferences.remove("state_running");
  preferences.remove("state_time");
  preferences.end();
}

String generateConfigHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Poker Timer Config</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
      color: #333;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      padding: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      text-align: center;
      color: #667eea;
      margin-bottom: 20px;
      font-size: 24px;
    }
    .device-mode {
      background: #e8eaf6;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 8px;
      border-left: 4px solid #667eea;
    }
    .device-mode h2 {
      font-size: 16px;
      color: #667eea;
      margin-bottom: 10px;
    }
    select {
      width: 100%;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 4px;
      font-size: 14px;
      background: white;
    }
    .mode-info {
      font-size: 12px;
      color: #666;
      margin-top: 8px;
      line-height: 1.4;
    }
    .round {
      background: #f8f9fa;
      padding: 15px;
      margin-bottom: 10px;
      border-radius: 8px;
      border-left: 4px solid #667eea;
    }
    .round.break {
      border-left-color: #ffc107;
      background: #fff9e6;
    }
    .round-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
      font-weight: bold;
      color: #667eea;
    }
    .round.break .round-header {
      color: #ff9800;
    }
    .form-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-bottom: 8px;
    }
    .form-group {
      display: flex;
      flex-direction: column;
    }
    label {
      font-size: 12px;
      color: #666;
      margin-bottom: 4px;
      font-weight: 500;
    }
    input[type="number"], input[type="text"] {
      padding: 8px;
      border: 1px solid #ddd;
      border-radius: 4px;
      font-size: 14px;
      width: 100%;
    }
    input[type="checkbox"] {
      width: 20px;
      height: 20px;
      cursor: pointer;
    }
    .checkbox-group {
      display: flex;
      align-items: center;
      gap: 8px;
    }
     .import-export {
      background: #e3f2fd;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 8px;
      border-left: 4px solid #2196f3;
    }
    .import-export h2 {
      font-size: 16px;
      color: #2196f3;
      margin-bottom: 10px;
    }
    .import-export-buttons {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 10px;
      margin-top: 10px;
    }
    .btn-export, .btn-import, .btn-reset {
      padding: 10px;
      font-size: 14px;
      font-weight: bold;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-export {
      background: #2196f3;
      color: white;
    }
    .btn-export:active {
      background: #1976d2;
    }
    .btn-import {
      background: #ff9800;
      color: white;
    }
    .btn-import:active {
      background: #f57c00;
    }
    .btn-reset {
      background: #9e9e9e;
      color: white;
    }
    .btn-reset:active {
      background: #757575;
    }
    #fileInput {
      display: none;
    }
    .buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-top: 20px;
      position: sticky;
      bottom: 20px;
    }
    button {
      padding: 15px;
      font-size: 16px;
      font-weight: bold;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-save {
      background: #4caf50;
      color: white;
    }
    .btn-save:active {
      background: #45a049;
    }
    .btn-cancel {
      background: #f44336;
      color: white;
    }
    .btn-cancel:active {
      background: #da190b;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎰 Poker Timer Config</h1>
    <form id="configForm">
      <div class="device-mode">
        <h2>Device Sync Mode</h2>
        <select name="deviceMode" id="deviceMode">
          <option value="0")rawliteral";
  
  if (deviceMode == MODE_STANDALONE) html += " selected";
  html += R"rawliteral(>Standalone (No Sync)</option>
          <option value="1")rawliteral";
  
  if (deviceMode == MODE_MASTER) html += " selected";
  html += R"rawliteral(>Master (Controls Others)</option>
          <option value="2")rawliteral";
  
  if (deviceMode == MODE_SLAVE) html += " selected";
  html += R"rawliteral(>Slave (Follows Master)</option>
        </select>
        <div class="mode-info" id="modeInfo">
          Select device synchronization mode using ESPNow.
        </div>
       </div>
       
       <div class="import-export">
        <h2>💾 Import / Export Configuration</h2>
        <div class="import-export-buttons">
          <button type="button" class="btn-export" onclick="exportConfig()">Export</button>
          <button type="button" class="btn-import" onclick="document.getElementById('fileInput').click()">Import</button>
          <button type="button" class="btn-reset" onclick="resetToDefaults()">Reset Defaults</button>
        </div>
        <input type="file" id="fileInput" accept=".json" onchange="importConfig(event)">
      </div>
)rawliteral";

  // Generate form fields for each round
  for (int i = 0; i < totalRounds; i++) {
    html += "<div class='round";
    if (rounds[i].isBreak) html += " break";
    html += "' id='round" + String(i) + "'><div class='round-header'><span>";
    
    html += "Round ";
    html += String(i + 1);
    html += "</span><div class='checkbox-group'><label style='margin:0'>Break?</label>";
    html += "<input type='checkbox' name='brk" + String(i) + "' onchange='toggleBreakStyle(" + String(i) + ")' " + 
            (rounds[i].isBreak ? "checked" : "") + "></div></div>";
    
    html += "<div class='form-row'>";
    html += "<div class='form-group'><label>Duration (min)</label>";
    html += "<input type='number' name='dur" + String(i) + "' value='" + 
            String(rounds[i].duration) + "' min='1' max='999'></div>";
    
    html += "<div class='form-group'><label>Small Blind</label>";
    html += "<input type='number' name='sb" + String(i) + "' value='" + 
            String(rounds[i].smallBlind) + "' min='0'></div></div>";
    
    html += "<div class='form-row'>";
    html += "<div class='form-group'><label>Big Blind</label>";
    html += "<input type='number' name='bb" + String(i) + "' value='" + 
            String(rounds[i].bigBlind) + "' min='0'></div>";
    
    html += "<div class='form-group'><label>Ante</label>";
    html += "<input type='number' name='ante" + String(i) + "' value='" + 
            String(rounds[i].ante) + "' min='0'></div></div></div>";
  }

  html += R"rawliteral(
    <div class="buttons">
      <button type="button" class="btn-cancel" onclick="cancel()">Cancel</button>
      <button type="submit" class="btn-save">Save</button>
    </div>
    </form>
  </div>
  <script>
    // Update mode info text when selection changes
    const deviceModeSelect = document.getElementById('deviceMode');
    const modeInfo = document.getElementById('modeInfo');
    
    const modeDescriptions = {
      '0': 'Standalone mode - no synchronization. Device operates independently.',
      '1': 'Master mode - controls the timer and broadcasts to all slave devices via ESPNow.',
      '2': 'Slave mode - follows master device. All control commands sent to master.'
    };
    
    deviceModeSelect.addEventListener('change', (e) => {
      modeInfo.textContent = modeDescriptions[e.target.value];
    });
    
    // Set initial description
    modeInfo.textContent = modeDescriptions[deviceModeSelect.value];
    
    // Toggle break style when checkbox changes
    function toggleBreakStyle(index) {
      const roundDiv = document.getElementById('round' + index);
      const checkbox = document.querySelector('[name="brk' + index + '"]');
      
      if (checkbox.checked) {
        roundDiv.classList.add('break');
      } else {
        roundDiv.classList.remove('break');
      }
    }
    
    document.getElementById('configForm').onsubmit = async (e) => {
      e.preventDefault();
      const formData = new FormData(e.target);
      const data = {};
      for (let [key, value] of formData.entries()) {
        data[key] = value;
      }
      
      // Include unchecked checkboxes as false
      const checkboxes = document.querySelectorAll('input[type="checkbox"]');
      checkboxes.forEach(cb => {
        if (!data[cb.name]) data[cb.name] = 'false';
        else data[cb.name] = 'true';
      });
      
      const response = await fetch('/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      });
      
      if (response.ok) {
        alert('Settings saved! Rebooting...');
      }
    };
    
    function cancel() {
      if (confirm('Cancel without saving?')) {
        fetch('/cancel').then(() => {
          alert('Rebooting...');
        });
      }
    }
    
    // Export configuration to JSON file
    function exportConfig() {
      const formData = new FormData(document.getElementById('configForm'));
      const config = {
        version: '1.0',
        exportDate: new Date().toISOString(),
        deviceMode: formData.get('deviceMode'),
        rounds: []
      };
      
      // Collect all round data
      for (let i = 0; i < 25; i++) {
        const round = {
          duration: parseInt(formData.get('dur' + i)),
          smallBlind: parseInt(formData.get('sb' + i)),
          bigBlind: parseInt(formData.get('bb' + i)),
          ante: parseInt(formData.get('ante' + i)),
          isBreak: formData.get('brk' + i) ? true : false
        };
        config.rounds.push(round);
      }
      
      // Create and download JSON file
      const json = JSON.stringify(config, null, 2);
      const blob = new Blob([json], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'poker_timer_config_' + new Date().toISOString().split('T')[0] + '.json';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      
      alert('Configuration exported successfully!');
    }
    
    // Import configuration from JSON file
    function importConfig(event) {
      const file = event.target.files[0];
      if (!file) return;
      
      const reader = new FileReader();
      reader.onload = function(e) {
        try {
          const config = JSON.parse(e.target.result);
          
          // Validate configuration
          if (!config.rounds || !Array.isArray(config.rounds)) {
            throw new Error('Invalid configuration format');
          }
          
          // Apply configuration to form
          if (config.deviceMode !== undefined) {
            document.querySelector('[name="deviceMode"]').value = config.deviceMode;
            document.getElementById('deviceMode').dispatchEvent(new Event('change'));
          }
          
          config.rounds.forEach((round, i) => {
            if (i < 25) {
              document.querySelector('[name="dur' + i + '"]').value = round.duration || 15;
              document.querySelector('[name="sb' + i + '"]').value = round.smallBlind || 0;
              document.querySelector('[name="bb' + i + '"]').value = round.bigBlind || 0;
              document.querySelector('[name="ante' + i + '"]').value = round.ante || 0;
              document.querySelector('[name="brk' + i + '"]').checked = round.isBreak || false;
              toggleBreakStyle(i); // Update visual styling
            }
          });
          
          alert('Configuration imported successfully! Review and click Save to apply.');
        } catch (err) {
          alert('Error importing configuration: ' + err.message);
        }
      };
      reader.readAsText(file);
      
      // Reset file input so the same file can be imported again
      event.target.value = '';
    }
    
    // Reset to default configuration
    function resetToDefaults() {
      if (!confirm('Reset to default blinds/breaks configuration? This will overwrite current values.')) {
        return;
      }
      
      // Default blinds structure (matches initializeDefaultRounds in C++)
      const blinds = [25, 50, 75, 100, 150, 200, 300, 400, 500, 600, 
                      800, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 
                      8000, 10000, 12000, 15000, 20000, 25000, 30000];
      
      let roundIndex = 0;
      for (let i = 0; i < 25; i++) {
        // Every 4th position is a break (after 3 rounds of play)
        const isBreak = (i + 1) % 4 === 0 && i > 0;
        
        document.querySelector('[name="dur' + i + '"]').value = 15;
        
        if (isBreak) {
          document.querySelector('[name="sb' + i + '"]').value = 0;
          document.querySelector('[name="bb' + i + '"]').value = 0;
          document.querySelector('[name="ante' + i + '"]').value = 0;
          document.querySelector('[name="brk' + i + '"]').checked = true;
        } else {
          document.querySelector('[name="sb' + i + '"]').value = blinds[roundIndex];
          document.querySelector('[name="bb' + i + '"]').value = blinds[roundIndex] * 2;
          document.querySelector('[name="ante' + i + '"]').value = 0;
          document.querySelector('[name="brk' + i + '"]').checked = false;
          roundIndex++;
        }
        toggleBreakStyle(i); // Update visual styling
      }
      
      alert('Reset to defaults! Review and click Save to apply.');
    }
  </script>
</body>
</html>
)rawliteral";

  return html;
}

// Buzzer functions
void playBeep(int frequency, int duration) {
  if (isMuted) return;  // Don't play sound if muted
  
  ledcSetup(0, frequency, 8);  // Channel 0, 8-bit resolution
  ledcAttachPin(BUZZER_PIN, 0);
  ledcWrite(0, 128);  // 50% duty cycle for loud sound
  delay(duration);
  ledcWrite(0, 0);  // Stop
  ledcDetachPin(BUZZER_PIN);
}

void playButtonBeep() {
  // Short, high-pitched beep for button press
  playBeep(2000, 50);  // 2kHz for 50ms
}

void playRoundEndBeeps() {
  // 5 seconds of beeping: pattern of 3 beeps, pause, repeat
  for (int i = 0; i < 5; i++) {  // 5 cycles = ~5 seconds
    playBeep(1500, 200);  // 1.5kHz for 200ms
    delay(100);
    playBeep(1500, 200);
    delay(100);
    playBeep(1500, 200);
    delay(400);  // Pause between cycles
  }
}

void startConfigMode() {
  configMode = true;
  
  Serial.println("\n\n=========================");
  Serial.println("ENTERING CONFIG MODE");
  Serial.println("=========================");
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(80, 40);
  tft.println("CONFIG MODE");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(90, 80);
  tft.println("Starting AP...");
  
  // Disconnect from any existing WiFi
  WiFi.disconnect(true);
  delay(100);
  
  // Start Access Point
  Serial.println("Setting WiFi mode to AP...");
  WiFi.mode(WIFI_AP);
  delay(500);
  
  Serial.println("\n--- Attempting to start SoftAP ---");
  Serial.println("SSID: PokerTimer");
  
  bool apStarted = WiFi.softAP("PokerTimer", "", 1, 0, 4);
  
  if (!apStarted) {
    Serial.println("Attempt failed. Trying simpler config...");
    delay(500);
    WiFi.softAPdisconnect(true);
    delay(500);
    apStarted = WiFi.softAP("PokerTimer");
  }
  
  if (!apStarted) {
    Serial.println("✗ FAILED! All attempts failed!");
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(90, 100);
    tft.setTextColor(TFT_RED);
    tft.println("AP FAILED!");
    while(true) { delay(1000); }
  }
  
  delay(1000);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("=========================\n");
  
  // Update display (landscape)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(80, 20);
  tft.println("CONFIG MODE");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(80, 50);
  tft.println("WiFi AP Ready!");
  tft.setCursor(80, 70);
  tft.println("Connect to:");
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(90, 90);
  tft.println("PokerTimer");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(80, 110);
  tft.print("IP: ");
  tft.println(IP);
  
  // Draw cancel button
  tft.setTextSize(1);
  tft.setCursor(60, 140);
  tft.println("Touch button below to");
  tft.setCursor(60, 155);
  tft.println("exit without saving:");
  drawButton(btnCancelConfig);
  
  // Setup web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", generateConfigHTML());
  });
  
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String jsonData = "";
      for (size_t i = 0; i < len; i++) {
        jsonData += (char)data[i];
      }
      
      Serial.println("Received config data:");
      Serial.println(jsonData);
      
      // Parse device mode
      String deviceModeKey = "\"deviceMode\":\"";
      int deviceModeIdx = jsonData.indexOf(deviceModeKey);
      if (deviceModeIdx >= 0) {
        deviceModeIdx += deviceModeKey.length();
        int modeValue = jsonData.substring(deviceModeIdx, jsonData.indexOf("\"", deviceModeIdx)).toInt();
        deviceMode = (DeviceMode)modeValue;
        Serial.print("Device mode set to: ");
        Serial.println(deviceMode == MODE_MASTER ? "MASTER" : deviceMode == MODE_SLAVE ? "SLAVE" : "STANDALONE");
      }
      
      // Parse the form data
      for (int i = 0; i < totalRounds; i++) {
        String durKey = "\"dur" + String(i) + "\":\"";
        String sbKey = "\"sb" + String(i) + "\":\"";
        String bbKey = "\"bb" + String(i) + "\":\"";
        String anteKey = "\"ante" + String(i) + "\":\"";
        String brkKey = "\"brk" + String(i) + "\":\"";
        
        int durIdx = jsonData.indexOf(durKey);
        if (durIdx >= 0) {
          durIdx += durKey.length();
          rounds[i].duration = jsonData.substring(durIdx, jsonData.indexOf("\"", durIdx)).toInt();
        }
        
        int sbIdx = jsonData.indexOf(sbKey);
        if (sbIdx >= 0) {
          sbIdx += sbKey.length();
          rounds[i].smallBlind = jsonData.substring(sbIdx, jsonData.indexOf("\"", sbIdx)).toInt();
        }
        
        int bbIdx = jsonData.indexOf(bbKey);
        if (bbIdx >= 0) {
          bbIdx += bbKey.length();
          rounds[i].bigBlind = jsonData.substring(bbIdx, jsonData.indexOf("\"", bbIdx)).toInt();
        }
        
        int anteIdx = jsonData.indexOf(anteKey);
        if (anteIdx >= 0) {
          anteIdx += anteKey.length();
          rounds[i].ante = jsonData.substring(anteIdx, jsonData.indexOf("\"", anteIdx)).toInt();
        }
        
        int brkIdx = jsonData.indexOf(brkKey);
        if (brkIdx >= 0) {
          brkIdx += brkKey.length();
          String brkVal = jsonData.substring(brkIdx, jsonData.indexOf("\"", brkIdx));
          rounds[i].isBreak = (brkVal == "true");
        }
      }
      
      saveRoundsToPreferences();
      playButtonBeep();
      request->send(200, "text/plain", "OK");
      
      // If master, broadcast new config to all slaves before rebooting
      if (deviceMode == MODE_MASTER && espNowInitialized) {
        Serial.println("Master config changed - broadcasting to slaves...");
        broadcastAllRoundConfigs();
        delay(1000);  // Give time for all messages to send
      }
      
      delay(500);
      ESP.restart();
    }
  );
  
  server.on("/cancel", HTTP_GET, [](AsyncWebServerRequest *request){
    playButtonBeep();
    request->send(200, "text/plain", "Rebooting");
    delay(500);
    ESP.restart();
  });
  
  server.begin();
  Serial.println("Web server started");
}

void drawButton(TouchButton &btn) {
  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, btn.color);
  tft.setTextColor(btn.textColor, btn.color); // Set background color to prevent overlap
  tft.setTextSize(2);
  
  // Calculate text position to center it
  int16_t textX = btn.x + (btn.w - (btn.label.length() * 12)) / 2; // Approximate 12px per char at size 2
  int16_t textY = btn.y + (btn.h - 16) / 2; // 16px height at size 2
  
  tft.setCursor(textX, textY);
  tft.print(btn.label);
}

bool isTouchInButton(int x, int y, TouchButton &btn) {
  return (x >= btn.x && x <= (btn.x + btn.w) && y >= btn.y && y <= (btn.y + btn.h));
}

void drawTimerDisplay() {
  // Prevent re-entrant calls (protection against race conditions)
  if (isDrawing) {
    Serial.println("WARNING: Skipping re-entrant drawTimerDisplay call");
    return;
  }
  isDrawing = true;
  
  tft.fillScreen(TFT_BLACK);
  
  // LANDSCAPE LAYOUT (320x240) - buttons on left, info on right
  
  // Draw vertical separator line between buttons and content
  tft.drawFastVLine(90, 0, SCREEN_HEIGHT, TFT_WHITE);
  
  // Draw touch buttons (on left side)
  if (timerRunning) {
    btnStartPause.label = "PAUSE";
    btnStartPause.color = TFT_RED;
  } else {
    btnStartPause.label = "START";
    btnStartPause.color = TFT_GREEN;
  }
  
  drawButton(btnStartPause);
  
  // Grey out NEXT/PREV on slave devices (not functional)
  if (deviceMode == MODE_SLAVE) {
    btnNext.color = TFT_DARKGREY;
    btnNext.textColor = 0x8410;  // Light grey color (RGB565)
    btnPrev.color = TFT_DARKGREY;
    btnPrev.textColor = 0x8410;  // Light grey color (RGB565)
  } else {
    btnNext.color = TFT_BLUE;
    btnNext.textColor = TFT_WHITE;
    btnPrev.color = TFT_ORANGE;
    btnPrev.textColor = TFT_BLACK;
  }
  
  drawButton(btnNext);
  drawButton(btnPrev);
  
  // CONTENT AREA (right side, starting from x=100)
  
  // Display round info (top right area)
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(100, 10);
  
  if (rounds[currentRound].isBreak) {
    tft.print("BREAK");
  } else {
    tft.print("Round ");
    tft.print(currentRound + 1);
  }
  
  // Display blinds or break text
  if (!rounds[currentRound].isBreak) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(100, 35);
    tft.print("Blinds:");
    
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(100, 50);
    tft.print(rounds[currentRound].smallBlind);
    tft.print("/");
    tft.print(rounds[currentRound].bigBlind);
    
    if (rounds[currentRound].ante > 0) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(100, 80);
      tft.print("Ante: ");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print(rounds[currentRound].ante);
    }
  } else {
    tft.setTextSize(3);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(110, 50);
    tft.print("BREAK");
  }
  
  // Draw time remaining (large, centered in content area)
  int minutes = remainingSeconds / 60;
  int seconds = remainingSeconds % 60;
  
  tft.setTextSize(6);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(120, 110);
  if (minutes < 10) tft.print("0");
  tft.print(minutes);
  tft.print(":");
  if (seconds < 10) tft.print("0");
  tft.print(seconds);
  
  // Status indicator (bottom right)
  tft.setTextSize(2);
  if (deviceMode == MODE_SLAVE) {
    // Slaves always show "SLAVE" in red
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(145, 190);
    tft.print("SLAVE");
  } else if (timerRunning) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(130, 190);
    tft.print("RUNNING");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(140, 190);
    tft.print("PAUSED");
  }
  
  // Sync status indicator (top left of content area)
  if (deviceMode == MODE_MASTER) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(100, 0);
    tft.print("M:");  // Master indicator
    tft.print(getActiveSlaveCount());  // Number of connected slaves
    tft.print("S");  // S for "Slaves"
  } else if (deviceMode == MODE_SLAVE) {
    tft.setTextSize(1);
    if (isSyncConnected()) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(100, 0);
      tft.print("S");  // Slave synced
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(100, 0);
      tft.print("X");  // Slave disconnected
    }
  }
  
  // Config button or next round info (top right corner)
  bool showGear = false;
  bool showNextRound = false;
  
  if (deviceMode == MODE_SLAVE) {
    // Slave: toggle between gear and next round
    showGear = !slaveShowNextRound;
    showNextRound = slaveShowNextRound;
  } else {
    // Master/Standalone: show gear when paused, next round when running
    showGear = !timerRunning;
    showNextRound = timerRunning;
  }
  
  if (showGear) {
    // Draw config button (gear icon in top right)
    tft.fillRoundRect(btnConfig.x, btnConfig.y, btnConfig.w, btnConfig.h, 8, btnConfig.color);
    // Draw a simple gear icon
    tft.drawCircle(btnConfig.x + 15, btnConfig.y + 15, 10, TFT_WHITE);
    tft.drawCircle(btnConfig.x + 15, btnConfig.y + 15, 6, TFT_WHITE);
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = btnConfig.x + 15 + cos(angle) * 8;
      int y1 = btnConfig.y + 15 + sin(angle) * 8;
      int x2 = btnConfig.x + 15 + cos(angle) * 12;
      int y2 = btnConfig.y + 15 + sin(angle) * 12;
      tft.drawLine(x1, y1, x2, y2, TFT_WHITE);
    }
  } else {
    // Clear the gear area when hidden
    tft.fillRect(btnConfig.x, btnConfig.y, btnConfig.w, btnConfig.h, TFT_BLACK);
  }
  
  if (showNextRound && currentRound < totalRounds - 1) {
    // Display next round info at top right
    tft.setTextSize(1);
    
    if (rounds[currentRound + 1].isBreak) {
      // Next round is a break
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setCursor(270, 10);
      tft.println("Next:");
      tft.setCursor(265, 22);
      tft.println("Break");
    } else {
      // Next round has blinds - display them
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(270, 10);
      tft.println("Next:");
      tft.setCursor(260, 22);
      tft.print(rounds[currentRound + 1].smallBlind);
      tft.print("/");
      tft.println(rounds[currentRound + 1].bigBlind);
    }
  }
  
  // Draw mute button in bottom right corner
  tft.fillRoundRect(btnMute.x, btnMute.y, btnMute.w, btnMute.h, 8, btnMute.color);
  
  // Draw speaker icon
  int centerX = btnMute.x + 15;
  int centerY = btnMute.y + 15;
  
  if (isMuted) {
    // Draw muted speaker (speaker with X)
    // Speaker cone
    tft.fillTriangle(centerX - 5, centerY - 4, centerX - 5, centerY + 4, centerX - 1, centerY + 2, TFT_WHITE);
    tft.fillTriangle(centerX - 5, centerY - 4, centerX - 5, centerY + 4, centerX - 1, centerY - 2, TFT_WHITE);
    // Speaker box
    tft.fillRect(centerX - 8, centerY - 2, 3, 4, TFT_WHITE);
    // X mark to indicate muted
    tft.drawLine(centerX + 2, centerY - 4, centerX + 8, centerY + 4, TFT_RED);
    tft.drawLine(centerX + 2, centerY + 4, centerX + 8, centerY - 4, TFT_RED);
    tft.drawLine(centerX + 3, centerY - 4, centerX + 9, centerY + 4, TFT_RED);
    tft.drawLine(centerX + 3, centerY + 4, centerX + 9, centerY - 4, TFT_RED);
  } else {
    // Draw unmuted speaker (speaker with sound waves)
    // Speaker cone
    tft.fillTriangle(centerX - 5, centerY - 4, centerX - 5, centerY + 4, centerX - 1, centerY + 2, TFT_WHITE);
    tft.fillTriangle(centerX - 5, centerY - 4, centerX - 5, centerY + 4, centerX - 1, centerY - 2, TFT_WHITE);
    // Speaker box
    tft.fillRect(centerX - 8, centerY - 2, 3, 4, TFT_WHITE);
    // Sound waves
    tft.drawCircle(centerX + 2, centerY, 3, TFT_WHITE);
    tft.drawCircle(centerX + 2, centerY, 5, TFT_WHITE);
    tft.drawCircle(centerX + 2, centerY, 7, TFT_WHITE);
  }
  
  // Reset drawing flag
  isDrawing = false;
}

void handleTouch() {
  unsigned long currentTime = millis();
  
  if (!ts.touched() || currentTime - lastTouch < TOUCH_DEBOUNCE) {
    return;
  }
  
  lastTouch = currentTime;
  
  TS_Point p = ts.getPoint();
  
  // Map touch coordinates for LANDSCAPE rotation 1 (320x240)
  // Standard CYD calibration for rotation 1
  int x = map(p.x, 200, 3700, 0, 320);  // Map to width 320
  int y = map(p.y, 240, 3800, 0, 240);  // Map to height 240
  
  // Constrain to screen bounds
  x = constrain(x, 0, SCREEN_WIDTH - 1);
  y = constrain(y, 0, SCREEN_HEIGHT - 1);
  
  Serial.print("Touch RAW: p.x=");
  Serial.print(p.x);
  Serial.print(" p.y=");
  Serial.print(p.y);
  Serial.print(" -> Mapped: x=");
  Serial.print(x);
  Serial.print(" y=");
  Serial.print(y);
  
  // Show which button area this corresponds to
  if (isTouchInButton(x, y, btnStartPause)) {
    Serial.print(" [START BUTTON]");
  } else if (isTouchInButton(x, y, btnNext)) {
    Serial.print(" [NEXT BUTTON]");
  } else if (isTouchInButton(x, y, btnPrev)) {
    Serial.print(" [PREV BUTTON]");
  } else if (isTouchInButton(x, y, btnConfig)) {
    Serial.print(" [CONFIG BUTTON]");
  } else if (isTouchInButton(x, y, btnMute)) {
    Serial.print(" [MUTE BUTTON]");
  } else {
    Serial.print(" [No button]");
  }
  Serial.println();
  
  // Check Mute button first
  if (isTouchInButton(x, y, btnMute)) {
    Serial.println("Mute button pressed");
    isMuted = !isMuted;  // Toggle mute state
    Serial.print("Mute is now: ");
    Serial.println(isMuted ? "ON" : "OFF");
    
    // Play a brief beep to confirm (only if unmuting)
    if (!isMuted) {
      playButtonBeep();
    }
    
    // Redraw the display to update the mute icon
    drawTimerDisplay();
    return;  // Exit to prevent other button processing
  }
  
  // Check Config button (gear icon)
  if (isTouchInButton(x, y, btnConfig)) {
    Serial.println("Config button pressed");
    
    // Set flag to prevent timer updates from overwriting this screen
    inSettingsScreen = true;
    
    // Wait for touch release to prevent false triggers
    while (ts.touched()) {
      delay(10);
    }
    delay(300); // Extra delay to ensure touch is fully released
    Serial.println("Touch released, showing config screen");
    
    // Save timer state before showing confirmation dialog
    bool wasRunning = timerRunning;
    unsigned long pausedAt = millis();
    int savedRemaining = remainingSeconds;
    
    // If timer was running, calculate current remaining seconds
    if (wasRunning) {
      unsigned long elapsed = (pausedAt - roundStartTime) / 1000;
      savedRemaining = startingSeconds - elapsed;
      Serial.print("Timer was running, saving state: ");
      Serial.print(savedRemaining);
      Serial.println(" seconds remaining");
    }
    
    // Show confirmation screen (landscape)
    tft.fillScreen(TFT_BLACK);
    
    // Draw three reboot mode buttons at top
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK);
    
    // Standalone button (left)
    tft.fillRoundRect(5, 5, 100, 30, 6, TFT_CYAN);
    tft.setCursor(15, 13);
    tft.println("Standalone");
    
    // Master button (center)
    tft.fillRoundRect(110, 5, 100, 30, 6, TFT_MAGENTA);
    tft.setCursor(132, 13);
    tft.println("Master");
    
    // Slave button (right)
    tft.fillRoundRect(215, 5, 100, 30, 6, TFT_ORANGE);
    tft.setCursor(240, 13);
    tft.println("Slave");
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(70, 50);
    tft.println("Enter Config");
    tft.setCursor(100, 80);
    tft.println("Mode?");
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(60, 110);
    tft.println("This will start WiFi AP");
    tft.setCursor(60, 125);
    tft.println("and pause the timer.");
    
    // Draw YES/NO buttons (landscape)
    tft.fillRoundRect(60, 165, 90, 50, 8, TFT_GREEN);
    tft.fillRoundRect(170, 165, 90, 50, 8, TFT_RED);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(80, 180);
    tft.println("YES");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(195, 180);
    tft.println("NO");
    
    // Wait for confirmation (10 second timeout)
    unsigned long confirmStart = millis();
    bool confirmed = false;
    bool cancelled = false;
    
    while (!confirmed && !cancelled && (millis() - confirmStart < 10000)) {
      if (ts.touched()) {
        delay(50); // Debounce
        TS_Point p2 = ts.getPoint();
        int x2 = map(p2.x, 200, 3700, 0, SCREEN_WIDTH);
        int y2 = map(p2.y, 240, 3800, 0, SCREEN_HEIGHT);
        
        Serial.print("Config screen touch: x2=");
        Serial.print(x2);
        Serial.print(" y2=");
        Serial.println(y2);
        
        // Check reboot mode buttons at top
        if (y2 >= 5 && y2 <= 35) {
          Serial.println("Touch in top button area");
          // Standalone button (left)
          if (x2 >= 5 && x2 <= 105) {
            Serial.println("STANDALONE BUTTON PRESSED");
            playButtonBeep();
            Serial.println("Rebooting into Standalone mode");
            deviceMode = MODE_STANDALONE;
            saveRoundsToPreferences();
            delay(500);
            ESP.restart();
          }
          // Master button (center)
          else if (x2 >= 110 && x2 <= 210) {
            Serial.println("MASTER BUTTON PRESSED");
            playButtonBeep();
            Serial.println("Rebooting into Master mode");
            deviceMode = MODE_MASTER;
            saveRoundsToPreferences();
            delay(500);
            ESP.restart();
          }
          // Slave button (right)
          else if (x2 >= 215 && x2 <= 315) {
            Serial.println("SLAVE BUTTON PRESSED");
            playButtonBeep();
            Serial.println("Rebooting into Slave mode");
            deviceMode = MODE_SLAVE;
            saveRoundsToPreferences();
            delay(500);
            ESP.restart();
          }
          else {
            Serial.println("Touch in top area but not on any button");
          }
        }
        // Check YES button (left) - landscape coords
        else if (x2 >= 60 && x2 <= 150 && y2 >= 165 && y2 <= 215) {
          confirmed = true;
          Serial.println("Config mode confirmed");
        }
        // Check NO button (right) - landscape coords
        else if (x2 >= 170 && x2 <= 260 && y2 >= 165 && y2 <= 215) {
          cancelled = true;
          Serial.println("Config mode cancelled");
        }
        else {
          Serial.println("Touch not on any recognized button");
        }
        
        // Wait for release
        while (ts.touched()) {
          delay(10);
        }
        delay(200);
      }
      delay(50);
    }
    
    if (confirmed) {
      // Enter config mode
      Serial.println("Entering config mode...");
      loadRoundsFromPreferences();
      inSettingsScreen = false;  // Clear flag before entering config mode
      startConfigMode();
      return; // Don't redraw timer screen
    } else {
      // Cancelled or timeout - restore timer state and return to timer
      Serial.println("Returning to timer");
      
      // Restore timer state properly
      remainingSeconds = savedRemaining;
      startingSeconds = savedRemaining;
      timerRunning = wasRunning;
      roundStartTime = millis(); // Reset the start time to now
      
      Serial.print("Timer state restored: running=");
      Serial.print(timerRunning);
      Serial.print(", remaining=");
      Serial.println(remainingSeconds);
      
      inSettingsScreen = false;  // Clear flag before redrawing timer
      drawTimerDisplay();
    }
  }
  // Check Start/Pause button
  else if (isTouchInButton(x, y, btnStartPause)) {
    Serial.println("Start/Pause pressed");
    playButtonBeep();
    
    if (deviceMode == MODE_SLAVE) {
      // Slave: toggle next round visibility
      slaveShowNextRound = !slaveShowNextRound;
      Serial.print("Slave: Next round display ");
      Serial.println(slaveShowNextRound ? "SHOWN" : "HIDDEN");
      
      // Wait for touch release to prevent double-trigger
      while (ts.touched()) {
        delay(10);
      }
      delay(200);  // Extra debounce delay
      
      Serial.println("Slave: Updating display...");
      drawTimerDisplay();
      Serial.println("Slave: Display updated successfully");
    } else {
      // Master or standalone executes locally
      timerRunning = !timerRunning;
      if (timerRunning) {
        startingSeconds = remainingSeconds;
        roundStartTime = millis();
        Serial.println("Timer started/resumed");
      } else {
        Serial.println("Timer paused");
      }
      saveTimerState();
      
      // Master broadcasts the change
      if (deviceMode == MODE_MASTER && espNowInitialized) {
        broadcastTimerState();
        broadcastBeep(2);  // Send beep command to slaves (start/stop beep)
      }
      
      drawTimerDisplay();
    }
  }
  // Check Next button
  else if (isTouchInButton(x, y, btnNext)) {
    // NEXT button disabled on slaves
    if (deviceMode == MODE_SLAVE) {
      Serial.println("Next pressed on slave - ignoring (button disabled)");
      return;
    }
    
    Serial.println("Next pressed");
    playButtonBeep();
    
    if (currentRound < totalRounds - 1) {
      // Show confirmation dialog (landscape)
      tft.fillScreen(TFT_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(70, 50);
      tft.println("Skip to Next");
      tft.setCursor(100, 80);
      tft.println("Round?");
      
      // Draw YES/NO buttons (landscape)
      tft.fillRoundRect(60, 130, 90, 50, 8, TFT_GREEN);
      tft.fillRoundRect(170, 130, 90, 50, 8, TFT_RED);
      
      tft.setTextSize(2);
      tft.setTextColor(TFT_BLACK);
      tft.setCursor(80, 145);
      tft.println("YES");
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(195, 145);
      tft.println("NO");
      
      // Wait for confirmation (10 second timeout)
      unsigned long confirmStart = millis();
      bool confirmed = false;
      bool cancelled = false;
      
      while (!confirmed && !cancelled && (millis() - confirmStart < 10000)) {
        if (ts.touched()) {
          delay(50); // Debounce
          TS_Point p2 = ts.getPoint();
          int x2 = map(p2.x, 200, 3700, 0, SCREEN_WIDTH);
          int y2 = map(p2.y, 240, 3800, 0, SCREEN_HEIGHT);
          
          // Check YES button (left) - landscape coords
          if (x2 >= 60 && x2 <= 150 && y2 >= 130 && y2 <= 180) {
            confirmed = true;
            playButtonBeep();
            Serial.println("Next round confirmed");
          }
          // Check NO button (right) - landscape coords
          else if (x2 >= 170 && x2 <= 260 && y2 >= 130 && y2 <= 180) {
            cancelled = true;
            playButtonBeep();
            Serial.println("Next round cancelled");
          }
          
          // Wait for release
          while (ts.touched()) {
            delay(10);
          }
          delay(200);
        }
        delay(50);
      }
      
      if (confirmed) {
        // Master or standalone executes locally
        currentRound++;
        remainingSeconds = rounds[currentRound].duration * 60;
        startingSeconds = remainingSeconds;
        roundStartTime = millis();
        Serial.print("Next round: ");
        Serial.println(currentRound + 1);
        saveTimerState();
        
        // Master broadcasts the change
        if (deviceMode == MODE_MASTER && espNowInitialized) {
          broadcastTimerState();
          broadcastBeep(2);  // Send beep command to slaves (confirmation beep)
        }
      }
      
      drawTimerDisplay();
    }
  }
  // Check Prev button
  else if (isTouchInButton(x, y, btnPrev)) {
    // PREV button disabled on slaves
    if (deviceMode == MODE_SLAVE) {
      Serial.println("Prev pressed on slave - ignoring (button disabled)");
      return;
    }
    
    Serial.println("Prev pressed");
    playButtonBeep();
    
    if (currentRound > 0) {
      // Show confirmation dialog (landscape)
      tft.fillScreen(TFT_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(50, 50);
      tft.println("Go to Previous");
      tft.setCursor(100, 80);
      tft.println("Round?");
      
      // Draw YES/NO buttons (landscape)
      tft.fillRoundRect(60, 130, 90, 50, 8, TFT_GREEN);
      tft.fillRoundRect(170, 130, 90, 50, 8, TFT_RED);
      
      tft.setTextSize(2);
      tft.setTextColor(TFT_BLACK);
      tft.setCursor(80, 145);
      tft.println("YES");
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(195, 145);
      tft.println("NO");
      
      // Wait for confirmation (10 second timeout)
      unsigned long confirmStart = millis();
      bool confirmed = false;
      bool cancelled = false;
      
      while (!confirmed && !cancelled && (millis() - confirmStart < 10000)) {
        if (ts.touched()) {
          delay(50); // Debounce
          TS_Point p2 = ts.getPoint();
          int x2 = map(p2.x, 200, 3700, 0, SCREEN_WIDTH);
          int y2 = map(p2.y, 240, 3800, 0, SCREEN_HEIGHT);
          
          // Check YES button (left) - landscape coords
          if (x2 >= 60 && x2 <= 150 && y2 >= 130 && y2 <= 180) {
            confirmed = true;
            playButtonBeep();
            Serial.println("Previous round confirmed");
          }
          // Check NO button (right) - landscape coords
          else if (x2 >= 170 && x2 <= 260 && y2 >= 130 && y2 <= 180) {
            cancelled = true;
            playButtonBeep();
            Serial.println("Previous round cancelled");
          }
          
          // Wait for release
          while (ts.touched()) {
            delay(10);
          }
          delay(200);
        }
        delay(50);
      }
      
      if (confirmed) {
        // Master or standalone executes locally
        currentRound--;
        remainingSeconds = rounds[currentRound].duration * 60;
        startingSeconds = remainingSeconds;
        roundStartTime = millis();
        Serial.print("Previous round: ");
        Serial.println(currentRound + 1);
        saveTimerState();
        
        // Master broadcasts the change
        if (deviceMode == MODE_MASTER && espNowInitialized) {
          broadcastTimerState();
          broadcastBeep(2);  // Send beep command to slaves (confirmation beep)
        }
      }
      
      drawTimerDisplay();
    }
  }
}

// ESPNow Helper Functions
String macToString(const uint8_t *mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void updateSlaveTracking(const uint8_t *macAddr) {
  Serial.print("updateSlaveTracking called for ");
  Serial.println(macToString(macAddr));
  
  // Find existing slave or add new one
  int freeSlot = -1;
  for (int i = 0; i < MAX_TRACKED_SLAVES; i++) {
    if (trackedSlaves[i].active && 
        memcmp(trackedSlaves[i].macAddress, macAddr, 6) == 0) {
      // Existing slave - update timestamp
      trackedSlaves[i].lastSeen = millis();
      Serial.println("  -> Existing slave, timestamp updated");
      return;
    }
    if (!trackedSlaves[i].active && freeSlot == -1) {
      freeSlot = i;
    }
  }
  
  // New slave - add to tracking
  if (freeSlot >= 0) {
    int oldCount = activeSlaveCount;
    memcpy(trackedSlaves[freeSlot].macAddress, macAddr, 6);
    trackedSlaves[freeSlot].lastSeen = millis();
    trackedSlaves[freeSlot].active = true;
    trackedSlaves[freeSlot].configSynced = false;  // New slave needs config
    activeSlaveCount++;
    Serial.print("  -> NEW SLAVE! Total slaves: ");
    Serial.println(activeSlaveCount);
    
    // Update display when slave count changes
    if (oldCount != activeSlaveCount && !isDrawing) {
      drawTimerDisplay();
    }
  } else {
    Serial.println("  -> ERROR: No free slot for new slave!");
  }
}

void cleanupInactiveSlaves() {
  unsigned long now = millis();
  int oldCount = activeSlaveCount;
  
  for (int i = 0; i < MAX_TRACKED_SLAVES; i++) {
    if (trackedSlaves[i].active) {
      if (now - trackedSlaves[i].lastSeen > 10000) {  // 10 second timeout
        Serial.print("Slave disconnected: ");
        Serial.println(macToString(trackedSlaves[i].macAddress));
        trackedSlaves[i].active = false;
        activeSlaveCount--;
      }
    }
  }
  
  // Update display when slave count changes
  if (oldCount != activeSlaveCount && !isDrawing) {
    Serial.print("Slave count changed from ");
    Serial.print(oldCount);
    Serial.print(" to ");
    Serial.println(activeSlaveCount);
    drawTimerDisplay();
  }
}

int getActiveSlaveCount() {
  return activeSlaveCount;
}

// ESPNow Functions
void initESPNow() {
  if (deviceMode == MODE_STANDALONE) {
    Serial.println("Device in STANDALONE mode - ESPNow disabled");
    return;
  }
  
  Serial.println("\n--- Initializing ESPNow ---");
  
  // Set WiFi to station mode (required for ESPNow)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Initialize ESPNow
  if (esp_now_init() != ESP_OK) {
    Serial.println("✗ ESPNow init failed!");
    espNowInitialized = false;
    return;
  }
  
  Serial.println("✓ ESPNow initialized successfully");
  espNowInitialized = true;
  
  // Register callbacks
  esp_now_register_send_cb(onESPNowDataSent);
  esp_now_register_recv_cb(onESPNowDataRecv);
  
  // Add broadcast peer (both master and slave need this)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // Use channel 0 for auto
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("✗ Failed to add broadcast peer");
  } else {
    if (deviceMode == MODE_MASTER) {
      Serial.println("✓ Broadcast peer added (Master will broadcast to all slaves)");
    } else {
      Serial.println("✓ Broadcast peer added (Slave can send to master and receive broadcasts)");
    }
  }
  
  Serial.print("Device mode: ");
  Serial.println(deviceMode == MODE_MASTER ? "MASTER" : "SLAVE");
  Serial.println("---------------------------\n");
}

void onESPNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: log send status for debugging
  // Serial.print("Send Status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void onESPNowDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (data_len < 1) return;
  
  uint8_t messageType = data[0];
  
  if (messageType == MSG_TIMER_STATE && deviceMode == MODE_SLAVE) {
    if (data_len == sizeof(TimerStateMessage)) {
      handleTimerStateMessage((const TimerStateMessage*)data);
    } else {
      Serial.print("Slave: Timer state message size mismatch. Expected ");
      Serial.print(sizeof(TimerStateMessage));
      Serial.print(", got ");
      Serial.println(data_len);
    }
  } else if (messageType == MSG_ROUND_CONFIG && deviceMode == MODE_SLAVE) {
    if (data_len == sizeof(RoundConfigMessage)) {
      handleRoundConfigMessage((const RoundConfigMessage*)data);
    }
  } else if (messageType == MSG_BEEP && deviceMode == MODE_SLAVE) {
    if (data_len == sizeof(BeepMessage)) {
      handleBeepMessage((const BeepMessage*)data);
    }
  } else if (messageType == MSG_HEARTBEAT && deviceMode == MODE_MASTER) {
    Serial.print("Master: Received MSG_HEARTBEAT, size ");
    Serial.print(data_len);
    Serial.print(" (expected ");
    Serial.print(sizeof(HeartbeatMessage));
    Serial.println(")");
    
    if (data_len == sizeof(HeartbeatMessage)) {
      Serial.print("Master: Processing heartbeat from ");
      Serial.println(macToString(mac_addr));
      // Track the slave that sent heartbeat
      updateSlaveTracking(mac_addr);
      handleHeartbeatMessage(mac_addr);
    } else {
      Serial.println("Master: Heartbeat size mismatch - ignored");
    }
  }
}

void broadcastTimerState() {
  if (!espNowInitialized || deviceMode != MODE_MASTER) return;
  
  TimerStateMessage msg;
  msg.messageType = MSG_TIMER_STATE;
  msg.currentRound = currentRound;
  msg.remainingSeconds = remainingSeconds;
  msg.timerRunning = timerRunning;
  msg.timestamp = millis();
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(msg));
  
  // Debug log (only log occasionally to avoid spam)
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 5000) {  // Log every 5 seconds
    lastLog = millis();
    Serial.print("Master TX: Round ");
    Serial.print(currentRound + 1);
    Serial.print(", Time ");
    Serial.print(remainingSeconds);
    Serial.print("s, ");
    Serial.print(timerRunning ? "RUNNING" : "PAUSED");
    Serial.print(" - Send ");
    Serial.println(result == ESP_OK ? "OK" : "FAILED");
  }
}

void broadcastRoundConfig(int roundIndex) {
  if (!espNowInitialized || deviceMode != MODE_MASTER) return;
  if (roundIndex < 0 || roundIndex >= totalRounds) return;
  
  RoundConfigMessage msg;
  msg.messageType = MSG_ROUND_CONFIG;
  msg.roundIndex = roundIndex;
  msg.duration = rounds[roundIndex].duration;
  msg.smallBlind = rounds[roundIndex].smallBlind;
  msg.bigBlind = rounds[roundIndex].bigBlind;
  msg.ante = rounds[roundIndex].ante;
  msg.isBreak = rounds[roundIndex].isBreak;
  msg.totalRounds = totalRounds;
  
  esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(msg));
}

void broadcastAllRoundConfigs() {
  if (!espNowInitialized || deviceMode != MODE_MASTER) return;
  
  Serial.print("Broadcasting all ");
  Serial.print(totalRounds);
  Serial.println(" round configs to slaves...");
  
  for (int i = 0; i < totalRounds; i++) {
    broadcastRoundConfig(i);
    delay(20);  // Small delay between messages to avoid flooding
  }
  
  Serial.println("All round configs broadcasted");
}

void syncSlaveConfigs() {
  // Check if any slaves need config sync
  if (!espNowInitialized || deviceMode != MODE_MASTER) return;
  
  bool needsSync = false;
  for (int i = 0; i < MAX_TRACKED_SLAVES; i++) {
    if (trackedSlaves[i].active && !trackedSlaves[i].configSynced) {
      needsSync = true;
      break;
    }
  }
  
  if (needsSync) {
    // Broadcast all round configs
    broadcastAllRoundConfigs();
    
    // Mark all active slaves as synced
    for (int i = 0; i < MAX_TRACKED_SLAVES; i++) {
      if (trackedSlaves[i].active) {
        trackedSlaves[i].configSynced = true;
      }
    }
  }
}

void sendHeartbeat() {
  if (!espNowInitialized || deviceMode != MODE_SLAVE) return;
  
  HeartbeatMessage msg;
  msg.messageType = MSG_HEARTBEAT;
  msg.slaveId = 0;  // For future use
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(msg));
  
  // Debug log every heartbeat for troubleshooting
  Serial.print("Slave: Heartbeat sent (");
  Serial.print(sizeof(msg));
  Serial.print(" bytes) - ");
  Serial.println(result == ESP_OK ? "OK" : "FAILED");
}

void handleTimerStateMessage(const TimerStateMessage *msg) {
  // Slave receives timer state update from master
  lastSyncReceived = millis();
  
  // Debug log (only print if state changed)
  static uint8_t lastRound = 255;
  static uint16_t lastSeconds = 0;
  static bool lastRunning = false;
  
  bool stateChanged = (msg->currentRound != lastRound) || 
                      (msg->remainingSeconds != lastSeconds) ||
                      (msg->timerRunning != lastRunning);
  
  if (stateChanged) {
    Serial.print("Slave RX: Round ");
    Serial.print(msg->currentRound + 1);
    Serial.print(", Time ");
    Serial.print(msg->remainingSeconds);
    Serial.print("s, ");
    Serial.println(msg->timerRunning ? "RUNNING" : "PAUSED");
    
    lastRound = msg->currentRound;
    lastSeconds = msg->remainingSeconds;
    lastRunning = msg->timerRunning;
  }
  
  // Update local state
  currentRound = msg->currentRound;
  
  // Check if we need to beep for countdown warning (5-4-3-2-1 seconds)
  // Only beep when remainingSeconds changes to one of these values
  if (msg->timerRunning && msg->remainingSeconds >= 1 && msg->remainingSeconds <= 5) {
    if (remainingSeconds != msg->remainingSeconds) {
      // Beep 3 times for countdown warning
      for (int i = 0; i < 3; i++) {
        playBeep(1500, 200);  // 1.5kHz for 200ms
        delay(100);
      }
    }
  }
  
  remainingSeconds = msg->remainingSeconds;
  timerRunning = msg->timerRunning;
  
  // Update display immediately on state change, otherwise throttle
  // Skip update if already drawing (prevents race condition)
  // Skip update if in settings screen (prevents overwriting mode selection)
  static unsigned long lastDisplayUpdate = 0;
  if (inSettingsScreen) {
    Serial.println("Slave: Skipping display update - in settings screen");
  } else if (!isDrawing && (stateChanged || (millis() - lastDisplayUpdate > 1000))) {
    lastDisplayUpdate = millis();
    drawTimerDisplay();
  } else if (isDrawing) {
    Serial.println("Slave: Skipping display update - already drawing");
  }
}

void handleRoundConfigMessage(const RoundConfigMessage *msg) {
  // Slave receives round configuration from master
  if (msg->roundIndex >= MAX_ROUNDS) return;
  
  // Update total rounds if changed
  if (msg->totalRounds != totalRounds) {
    totalRounds = msg->totalRounds;
  }
  
  // Update round configuration
  rounds[msg->roundIndex].duration = msg->duration;
  rounds[msg->roundIndex].smallBlind = msg->smallBlind;
  rounds[msg->roundIndex].bigBlind = msg->bigBlind;
  rounds[msg->roundIndex].ante = msg->ante;
  rounds[msg->roundIndex].isBreak = msg->isBreak;
  
  Serial.print("Slave received config for round ");
  Serial.println(msg->roundIndex + 1);
}

void handleHeartbeatMessage(const uint8_t *senderMac) {
  // Master receives heartbeat from slave - just track it
  // updateSlaveTracking already called in onESPNowDataRecv
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 10000) {  // Log every 10 seconds
    lastLog = millis();
    Serial.print("Master: Heartbeat from ");
    Serial.print(macToString(senderMac));
    Serial.print(" - Total slaves: ");
    Serial.println(getActiveSlaveCount());
  }
}

void broadcastBeep(uint8_t beepType) {
  if (!espNowInitialized || deviceMode != MODE_MASTER) return;
  
  BeepMessage msg;
  msg.messageType = MSG_BEEP;
  msg.beepType = beepType;
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(msg));
  
  Serial.print("Master: Broadcasting beep command (type ");
  Serial.print(beepType);
  Serial.print(") - ");
  Serial.println(result == ESP_OK ? "OK" : "FAILED");
}

void handleBeepMessage(const BeepMessage *msg) {
  // Slave receives beep command from master
  Serial.print("Slave: Received beep command (type ");
  Serial.print(msg->beepType);
  Serial.println(")");
  
  // Play the appropriate beep
  if (msg->beepType == 0) {
    // Button beep
    playButtonBeep();
  } else if (msg->beepType == 1) {
    // Countdown warning (3 beeps)
    for (int i = 0; i < 3; i++) {
      playBeep(1500, 200);  // 1.5kHz for 200ms
      delay(100);
    }
  } else if (msg->beepType == 2) {
    // Start/stop or confirmation beep
    playButtonBeep();
  }
}

bool isSyncConnected() {
  if (deviceMode == MODE_STANDALONE || !espNowInitialized) {
    return false;
  }
  
  if (deviceMode == MODE_MASTER) {
    return true;  // Master is always "connected"
  }
  
  // Slave: check if we've received sync recently
  return (millis() - lastSyncReceived) < SYNC_TIMEOUT;
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);
  
  // Print startup banner
  Serial.println();
  Serial.println();
  Serial.println("==========================================");
  Serial.println("   POKER TOURNAMENT TIMER - CYD");
  Serial.println("   Build: " __DATE__ " " __TIME__);
  Serial.println("==========================================");
  Serial.println();
  
  // Initialize NVS
  Serial.println("\n--- Initializing NVS ---");
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.println("NVS partition corrupted or wrong version, erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  
  if (err == ESP_OK) {
    Serial.println("✓ NVS initialized successfully");
    nvsAvailable = true;
  } else {
    Serial.print("✗ NVS initialization failed: ");
    Serial.println(err);
    Serial.println("⚠ Will operate without saved settings");
    nvsAvailable = false;
  }
  Serial.println("------------------------\n");
  
  // Initialize buzzer
  Serial.println("\n--- Initializing Buzzer ---");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("✓ Buzzer pin initialized (GPIO 26)");
  Serial.println("---------------------------\n");
  
  // Initialize display
  Serial.println("\n--- Initializing Display ---");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("Backlight ON");
  
  tft.init();
  Serial.println("Display init() called");
  
  // Use rotation 1 for LANDSCAPE orientation
  tft.setRotation(1);
  Serial.print("Display rotation 1 (landscape): ");
  Serial.print(tft.width());
  Serial.print("x");
  Serial.println(tft.height());
  
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display initialization complete");
  Serial.print("Display size: ");
  Serial.print(tft.width());
  Serial.print(" x ");
  Serial.println(tft.height());
  Serial.println("---------------------------\n");
  
  // Initialize touch screen with SEPARATE SPI bus
  Serial.println("\n--- Initializing Touch Screen ---");
  
  // Initialize touch SPI bus (separate from display!)
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  Serial.println("✓ Touch SPI bus initialized (separate from display)");
  
  ts.begin(touchSPI);
  Serial.println("✓ ts.begin() completed with touchSPI");
  
  ts.setRotation(1); // Match display rotation (landscape)
  Serial.println("✓ Touch rotation set to 1 (landscape)");
  
  // Clear any initial touch noise with timeout
  Serial.println("Clearing initial touch noise...");
  delay(200);
  unsigned long clearStart = millis();
  int cleared = 0;
  while (ts.touched() && (millis() - clearStart < 1000)) {
    ts.getPoint(); // Clear the touch
    cleared++;
    delay(50);
  }
  Serial.print("✓ Touch stabilized (cleared ");
  Serial.print(cleared);
  Serial.println(" readings)");
  Serial.println("--------------------------------\n");
  
  // Show startup tip (landscape)
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(60, 100);
  tft.println("Tap gear icon (top-right)");
  tft.setCursor(70, 115);
  tft.println("to configure timer");
  
  delay(2000);
  
  // Normal operation mode
  Serial.println(">>> Starting NORMAL OPERATION mode <<<");
  
  // IMPORTANT: Load preferences FIRST to get device mode
  loadRoundsFromPreferences();
  loadTimerState();
  
  // Initialize slave tracking array
  for (int i = 0; i < MAX_TRACKED_SLAVES; i++) {
    trackedSlaves[i].active = false;
    trackedSlaves[i].configSynced = false;
  }
  activeSlaveCount = 0;
  
  // Initialize ESPNow AFTER loading device mode from preferences
  initESPNow();
  
  drawTimerDisplay();
}

void loop() {
  if (configMode) {
    // Config mode - keep web server alive and check for cancel button
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 10000) {
      lastStatus = millis();
      Serial.print("Config mode... Stations connected: ");
      Serial.println(WiFi.softAPgetStationNum());
    }
    
    // Check for cancel button touch
    unsigned long currentTime = millis();
    static unsigned long lastConfigTouch = 0;
    
    if (ts.touched() && (currentTime - lastConfigTouch > TOUCH_DEBOUNCE)) {
      lastConfigTouch = currentTime;
      
      TS_Point p = ts.getPoint();
      int x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
      int y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);
      
      // Check if cancel button was pressed
      if (isTouchInButton(x, y, btnCancelConfig)) {
        Serial.println("Cancel button pressed in config mode");
        playButtonBeep();
        
        // Show confirmation (landscape)
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(70, 40);
        tft.println("Exit Config");
        tft.setCursor(100, 70);
        tft.println("Mode?");
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 100);
        tft.println("Changes will NOT be saved");
        
        // Draw YES/NO buttons (landscape)
        tft.fillRoundRect(60, 140, 90, 50, 8, TFT_GREEN);
        tft.fillRoundRect(170, 140, 90, 50, 8, TFT_RED);
        
        tft.setTextSize(2);
        tft.setTextColor(TFT_BLACK);
        tft.setCursor(80, 155);
        tft.println("YES");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(195, 155);
        tft.println("NO");
        
        // Wait for confirmation
        unsigned long confirmStart = millis();
        bool confirmed = false;
        bool cancelled = false;
        
        while (!confirmed && !cancelled && (millis() - confirmStart < 10000)) {
          if (ts.touched()) {
            delay(50);
            TS_Point p2 = ts.getPoint();
            int x2 = map(p2.x, 200, 3700, 0, SCREEN_WIDTH);
            int y2 = map(p2.y, 240, 3800, 0, SCREEN_HEIGHT);
            
            // Check YES button - landscape coords
            if (x2 >= 60 && x2 <= 150 && y2 >= 140 && y2 <= 190) {
              confirmed = true;
              Serial.println("Exit config confirmed - rebooting");
              ESP.restart();
            }
            // Check NO button - landscape coords
            else if (x2 >= 170 && x2 <= 260 && y2 >= 140 && y2 <= 190) {
              cancelled = true;
              Serial.println("Exit cancelled - staying in config mode");
              // Redraw config mode screen (landscape)
              tft.fillScreen(TFT_BLACK);
              tft.setTextSize(2);
              tft.setTextColor(TFT_GREEN);
              tft.setCursor(80, 20);
              tft.println("CONFIG MODE");
              tft.setTextSize(1);
              tft.setTextColor(TFT_WHITE);
              tft.setCursor(80, 50);
              tft.println("WiFi AP Ready!");
              tft.setCursor(80, 70);
              tft.println("Connect to:");
              tft.setTextColor(TFT_CYAN);
              tft.setCursor(90, 90);
              tft.println("PokerTimer");
              tft.setTextColor(TFT_WHITE);
              tft.setCursor(80, 110);
              tft.print("IP: ");
              tft.println(WiFi.softAPIP());
              tft.setTextSize(1);
              tft.setCursor(60, 140);
              tft.println("Touch button below to");
              tft.setCursor(60, 155);
              tft.println("exit without saving:");
              drawButton(btnCancelConfig);
            }
            
            // Wait for release
            while (ts.touched()) {
              delay(10);
            }
            delay(200);
          }
          delay(50);
        }
        
        // If timeout, return to config mode (landscape)
        if (!confirmed && !cancelled) {
          tft.fillScreen(TFT_BLACK);
          tft.setTextSize(2);
          tft.setTextColor(TFT_GREEN);
          tft.setCursor(80, 20);
          tft.println("CONFIG MODE");
          tft.setTextSize(1);
          tft.setTextColor(TFT_WHITE);
          tft.setCursor(80, 50);
          tft.println("WiFi AP Ready!");
          tft.setCursor(80, 70);
          tft.println("Connect to:");
          tft.setTextColor(TFT_CYAN);
          tft.setCursor(90, 90);
          tft.println("PokerTimer");
          tft.setTextColor(TFT_WHITE);
          tft.setCursor(80, 110);
          tft.print("IP: ");
          tft.println(WiFi.softAPIP());
          tft.setTextSize(1);
          tft.setCursor(60, 140);
          tft.println("Touch button below to");
          tft.setCursor(60, 155);
          tft.println("exit without saving:");
          drawButton(btnCancelConfig);
        }
      }
    }
    
    delay(100);
    yield();
  } else {
    // Normal timer operation
    handleTouch();
    
    // Save state every 10 seconds (master only, slaves sync from master)
    if (deviceMode != MODE_SLAVE) {
      static unsigned long lastSave = 0;
      if (millis() - lastSave > 10000) {
        lastSave = millis();
        saveTimerState();
      }
    }
    
    // Master: Broadcast timer state every second and cleanup inactive slaves
    if (deviceMode == MODE_MASTER && espNowInitialized) {
      static unsigned long lastBroadcast = 0;
      if (millis() - lastBroadcast > 1000) {
        lastBroadcast = millis();
        broadcastTimerState();
      }
      
      // Cleanup inactive slaves every 5 seconds
      static unsigned long lastCleanup = 0;
      if (millis() - lastCleanup > 5000) {
        lastCleanup = millis();
        cleanupInactiveSlaves();
      }
      
      // Sync round configs to new slaves every 2 seconds
      static unsigned long lastConfigSync = 0;
      if (millis() - lastConfigSync > 2000) {
        lastConfigSync = millis();
        syncSlaveConfigs();
      }
    }
    
    // Slave: Send heartbeat every 5 seconds
    if (deviceMode == MODE_SLAVE && espNowInitialized) {
      static unsigned long lastHeartbeat = 0;
      if (millis() - lastHeartbeat > 5000) {
        lastHeartbeat = millis();
        sendHeartbeat();
      }
    }
    
    // Update timer if running (master only, slaves receive updates)
    if (timerRunning && deviceMode != MODE_SLAVE) {
      unsigned long currentTime = millis();
      unsigned long elapsed = (currentTime - roundStartTime) / 1000;
      int newRemaining = startingSeconds - elapsed;
      
      if (newRemaining != remainingSeconds) {
        remainingSeconds = newRemaining;
        
        // Beep 3 times when timer reaches 5, 4, 3, 2, or 1 seconds
        if (remainingSeconds >= 1 && remainingSeconds <= 5) {
          for (int i = 0; i < 3; i++) {
            playBeep(1500, 200);  // 1.5kHz for 200ms
            delay(100);
          }
        }
        
        if (remainingSeconds <= 0) {
          // Round finished
          remainingSeconds = 0;
          timerRunning = false;
          saveTimerState();
          
          Serial.println("Round finished!");
          
          drawTimerDisplay();
          
          // Auto-advance to next round immediately
          if (currentRound < totalRounds - 1) {
            currentRound++;
            remainingSeconds = rounds[currentRound].duration * 60;
            startingSeconds = remainingSeconds;
            roundStartTime = millis();
            timerRunning = true;
            saveTimerState();
            
            // Master: broadcast the round change
            if (deviceMode == MODE_MASTER && espNowInitialized) {
              broadcastTimerState();
            }
          }
        }
        
        drawTimerDisplay();
      }
    }
    
    delay(100);
  }
}
