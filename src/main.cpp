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
    return;
  }
  
  // Try to open preferences
  bool opened = preferences.begin("poker-timer", true);
  
  if (!opened) {
    Serial.println("✗ Failed to open preferences (this is normal on first run)");
    Serial.println("Using default configuration");
    initializeDefaultRounds();
    preferences.end();
    return;
  }
  
  Serial.println("✓ Preferences opened successfully");
  
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
)rawliteral";

  // Generate form fields for each round
  for (int i = 0; i < totalRounds; i++) {
    html += "<div class='round";
    if (rounds[i].isBreak) html += " break";
    html += "'><div class='round-header'><span>";
    
    if (rounds[i].isBreak) {
      html += "BREAK ";
    } else {
      html += "Round ";
    }
    html += String(i + 1);
    html += "</span><div class='checkbox-group'><label style='margin:0'>Break?</label>";
    html += "<input type='checkbox' name='brk" + String(i) + "' " + 
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
  </script>
</body>
</html>
)rawliteral";

  return html;
}

// Buzzer functions
void playBeep(int frequency, int duration) {
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
  if (timerRunning) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(130, 190);
    tft.print("RUNNING");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(140, 190);
    tft.print("PAUSED");
  }
  
  // Config button or next round info (top right corner)
  if (!timerRunning) {
    // Draw config button (gear icon in top right) - only when paused
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
    // When running, display next round info at top right
    if (currentRound < totalRounds - 1) {
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
  }
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
  } else {
    Serial.print(" [No button]");
  }
  Serial.println();
  
  // Check Config button (gear icon)
  if (isTouchInButton(x, y, btnConfig)) {
    Serial.println("Config button pressed");
    
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
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(70, 40);
    tft.println("Enter Config");
    tft.setCursor(100, 70);
    tft.println("Mode?");
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(60, 100);
    tft.println("This will start WiFi AP");
    tft.setCursor(60, 115);
    tft.println("and pause the timer.");
    
    // Draw YES/NO buttons (landscape)
    tft.fillRoundRect(60, 150, 90, 50, 8, TFT_GREEN);
    tft.fillRoundRect(170, 150, 90, 50, 8, TFT_RED);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(80, 165);
    tft.println("YES");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(195, 165);
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
        if (x2 >= 60 && x2 <= 150 && y2 >= 150 && y2 <= 200) {
          confirmed = true;
          Serial.println("Config mode confirmed");
        }
        // Check NO button (right) - landscape coords
        else if (x2 >= 170 && x2 <= 260 && y2 >= 150 && y2 <= 200) {
          cancelled = true;
          Serial.println("Config mode cancelled");
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
      
      drawTimerDisplay();
    }
  }
  // Check Start/Pause button
  else if (isTouchInButton(x, y, btnStartPause)) {
    Serial.println("Start/Pause pressed");
    playButtonBeep();
    timerRunning = !timerRunning;
    if (timerRunning) {
      startingSeconds = remainingSeconds;
      roundStartTime = millis();
      Serial.println("Timer started/resumed");
    } else {
      Serial.println("Timer paused");
    }
    saveTimerState();
    drawTimerDisplay();
  }
  // Check Next button
  else if (isTouchInButton(x, y, btnNext)) {
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
        currentRound++;
        remainingSeconds = rounds[currentRound].duration * 60;
        startingSeconds = remainingSeconds;
        roundStartTime = millis();
        Serial.print("Next round: ");
        Serial.println(currentRound + 1);
        saveTimerState();
      }
      
      drawTimerDisplay();
    }
  }
  // Check Prev button
  else if (isTouchInButton(x, y, btnPrev)) {
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
        currentRound--;
        remainingSeconds = rounds[currentRound].duration * 60;
        startingSeconds = remainingSeconds;
        roundStartTime = millis();
        Serial.print("Previous round: ");
        Serial.println(currentRound + 1);
        saveTimerState();
      }
      
      drawTimerDisplay();
    }
  }
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
  loadRoundsFromPreferences();
  loadTimerState();
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
    
    // Save state every 10 seconds
    static unsigned long lastSave = 0;
    if (millis() - lastSave > 10000) {
      lastSave = millis();
      saveTimerState();
    }
    
    // Update timer if running
    if (timerRunning) {
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
          }
        }
        
        drawTimerDisplay();
      }
    }
    
    delay(100);
  }
}
