/*
 * Gas Monitoring System - SERVER FIRMWARE (IMPROVED)
 * ESP8266 Wemos D1 mini with SSD1306 Display and 4 LEDs
 * 
 * Acts as WiFi Access Point and displays data from all 4 client devices
 * Display divided into 4 sections with large digits and alert background invert
 * 
 * IMPROVED: Better diagnostics, client timeout detection, proper blink sync
 */

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ============================================
// Server Configuration
// ============================================
#define MAX_CLIENTS 4
#define DEVICE_ID 0  // Server device ID
#define CLIENT_TIMEOUT 5000  // ms - time to consider client disconnected

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// WiFi Server
WiFiServer wifiServer(SERVER_PORT);
WiFiClient serverClients[MAX_CLIENTS];

// ============================================
// Server State
// ============================================
struct ClientData {
  uint16_t gasLevel;
  bool connected;
  bool gasAlert;
  unsigned long lastUpdate;
  unsigned long lastBlinkToggle;
  bool ledState;
  bool displayInverted;  // For alert background invert effect
};

ClientData clients[MAX_CLIENTS] = {
  {0, false, false, 0, 0, false, false},
  {0, false, false, 0, 0, false, false},
  {0, false, false, 0, 0, false, false},
  {0, false, false, 0, 0, false, false}
};

// Timing
unsigned long lastDisplayUpdate = 0;
unsigned long lastStatusUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 100  // Faster update for smooth blink
#define STATUS_UPDATE_INTERVAL 1000  // Print status every second

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║  Gas Monitor SERVER (Improved)  ║");
  Serial.println("╚════════════════════════════════╝\n");
  
  // Initialize I2C and Display
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println("❌ SSD1306 allocation failed");
    Serial.println("   Check I2C wiring:");
    Serial.println("   - D1 (GPIO5) -> SCL");
    Serial.println("   - D2 (GPIO4) -> SDA");
    Serial.println("   - Display address: 0x3C");
    while(1) {
      delay(100);
    }
  }
  
  Serial.println("✅ Display initialized");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Gas Monitor Server");
  display.println("Starting WiFi AP...");
  display.display();
  
  // Initialize LED pins
  pinMode(D3, OUTPUT);  // LED for device 1
  pinMode(D4, OUTPUT);  // LED for device 2
  pinMode(D5, OUTPUT);  // LED for device 3
  pinMode(D6, OUTPUT);  // LED for device 4
  
  digitalWrite(D3, LOW);
  digitalWrite(D4, LOW);
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  
  Serial.println("✅ LED pins initialized");
  
  // Start WiFi AP
  setupWiFiAP();
  
  // Start server
  wifiServer.begin();
  Serial.println("✅ WiFi server started on port 8888\n");
  
  delay(500);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi AP Active");
  display.print("SSID: ");
  display.println(WIFI_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.display();
  
  Serial.println("Setup complete!");
  Serial.println("Waiting for client connections...\n");
}

// ============================================
// Main Loop
// ============================================
void loop() {
  unsigned long now = millis();
  
  // Handle new client connections
  handleNewConnections();
  
  // Read data from connected clients
  readClientData();
  
  // Check for stale connections
  checkConnectionTimeout(now);
  
  // Update LED states
  updateLEDs(now);
  
  // Update display with alert animation
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay(now);
    lastDisplayUpdate = now;
  }
  
  // Print status periodically
  if (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    printStatus();
    lastStatusUpdate = now;
  }
  
  delay(10);
}

// ============================================
// WiFi AP Setup
// ============================================
void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  bool result = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  WiFi.softAPConfig(SERVER_IP_ADDR, SERVER_IP_ADDR, IPAddress(255, 255, 255, 0));
  
  Serial.print("📡 AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  if (!result) {
    Serial.println("❌ Failed to start WiFi AP");
  }
}

// ============================================
// Handle New Client Connections
// ============================================
void handleNewConnections() {
  WiFiClient newClient = wifiServer.available();
  
  if (newClient) {
    Serial.println("\n🔗 New client connection!");
    
    // Find empty slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!serverClients[i].connected()) {
        serverClients[i] = newClient;
        clients[i].connected = true;
        clients[i].lastUpdate = millis();
        Serial.print("✅ Client assigned to slot ");
        Serial.println(i + 1);
        Serial.println();
        
        // Turn on LED for this device
        updateDeviceLED(i, true, false);
        break;
      }
    }
  }
}

// ============================================
// Read Data from Clients
// ============================================
void readClientData() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i].connected()) {
      while (serverClients[i].available()) {
        String line = serverClients[i].readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) continue;
        
        // Parse messages
        if (line.startsWith("REG:")) {
          // Registration message
          int deviceId = line.substring(4).toInt();
          Serial.print("Device ");
          Serial.print(deviceId);
          Serial.println(" registered");
          
        } else if (line.startsWith("DATA:")) {
          // Data message: DATA:device_id:gas_level
          int firstColon = line.indexOf(':');
          int secondColon = line.indexOf(':', firstColon + 1);
          
          if (secondColon > 0) {
            int deviceId = line.substring(firstColon + 1, secondColon).toInt();
            uint16_t gasLevel = line.substring(secondColon + 1).toInt();
            
            if (deviceId >= 1 && deviceId <= MAX_CLIENTS) {
              int idx = deviceId - 1;
              clients[idx].gasLevel = gasLevel;
              clients[idx].gasAlert = (gasLevel >= GAS_THRESHOLD);
              clients[idx].lastUpdate = millis();
              
              Serial.print("📊 Device ");
              Serial.print(deviceId);
              Serial.print(" - Gas: ");
              Serial.print(gasLevel);
              Serial.print(" - Alert: ");
              Serial.println(clients[idx].gasAlert ? "⚠️  YES" : "✅ NO");
            }
          }
        }
      }
    }
  }
}

// ============================================
// Check Connection Timeout
// ============================================
void checkConnectionTimeout(unsigned long now) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    // Check if client should be marked as disconnected
    if (clients[i].connected && (now - clients[i].lastUpdate > CLIENT_TIMEOUT)) {
      if (!serverClients[i].connected()) {
        clients[i].connected = false;
        clients[i].gasAlert = false;
        clients[i].displayInverted = false;
        
        Serial.print("⚠️  Device ");
        Serial.print(i + 1);
        Serial.println(" disconnected (timeout)");
      }
    }
  }
}

// ============================================
// Update Display with 4-section layout
// ============================================
void updateDisplay(unsigned long now) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Display 4 sections (2x2 grid)
  // Each section: 64x32 pixels
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    int col = i % 2;  // 0 or 1 (left or right)
    int row = i / 2;  // 0 or 1 (top or bottom)
    
    int x = col * 64;
    int y = row * 32;
    
    // Handle alert background invert (200ms blink period)
    bool isBlinkOn = (now / 200) % 2 == 0;  // Toggle every 200ms
    
    if (clients[i].gasAlert && clients[i].connected) {
      // Gas detected - invert background
      if (isBlinkOn) {
        // White background, black text
        display.fillRect(x, y, 64, 32, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        // Black background, white text (normal)
        display.fillRect(x, y, 64, 32, SSD1306_BLACK);
        display.setTextColor(SSD1306_WHITE);
      }
    } else {
      // Normal state - white text on black background
      display.fillRect(x, y, 64, 32, SSD1306_BLACK);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // Draw section border
    display.drawRect(x, y, 64, 32, SSD1306_WHITE);
    
    // Display device number
    display.setTextSize(1);
    display.setCursor(x + 4, y + 2);
    display.print("D");
    display.println(i + 1);
    
    // Display gas level (large digits)
    display.setTextSize(2);
    display.setCursor(x + 8, y + 12);
    
    if (clients[i].connected) {
      // Pad with leading space for single/double digit numbers
      if (clients[i].gasLevel < 1000) {
        display.print(" ");
      }
      display.println(clients[i].gasLevel);
    } else {
      display.println("  --");
    }
  }
  
  display.display();
}

// ============================================
// Update LEDs
// ============================================
void updateLEDs(unsigned long now) {
  int ledPins[MAX_CLIENTS] = {D3, D4, D5, D6};
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].gasAlert) {
      // Blink LED if gas detected (200ms period)
      bool isBlinkOn = (now / 200) % 2 == 0;
      digitalWrite(ledPins[i], isBlinkOn ? HIGH : LOW);
      
    } else if (clients[i].connected) {
      // Keep LED on if connected but no alert
      digitalWrite(ledPins[i], HIGH);
    } else {
      // Turn off LED if not connected
      digitalWrite(ledPins[i], LOW);
    }
  }
}

// ============================================
// Update Device LED (direct control)
// ============================================
void updateDeviceLED(int deviceIdx, bool connected, bool alert) {
  int ledPins[MAX_CLIENTS] = {D3, D4, D5, D6};
  
  if (connected) {
    digitalWrite(ledPins[deviceIdx], HIGH);
  } else {
    digitalWrite(ledPins[deviceIdx], LOW);
  }
}

// ============================================
// Print Status
// ============================================
void printStatus() {
  int connectedCount = 0;
  int alertCount = 0;
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].connected) connectedCount++;
    if (clients[i].gasAlert) alertCount++;
  }
  
  Serial.print("📊 Status: ");
  Serial.print(connectedCount);
  Serial.print(" connected, ");
  Serial.print(alertCount);
  Serial.println(" alerts");
}

// ============================================
// Serial Debug
// ============================================
void serialEvent() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "status") {
      Serial.println("\n╔════════════════════════════════╗");
      Serial.println("║       SERVER STATUS REPORT       ║");
      Serial.println("╚════════════════════════════════╝");
      
      Serial.print("WiFi AP IP: ");
      Serial.println(WiFi.softAPIP());
      
      Serial.print("Connected Clients: ");
      int connectedCount = 0;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) connectedCount++;
      }
      Serial.println(connectedCount);
      Serial.println();
      
      for (int i = 0; i < MAX_CLIENTS; i++) {
        Serial.print("Device ");
        Serial.print(i + 1);
        Serial.print(": ");
        if (clients[i].connected) {
          Serial.print("✅ Connected - Gas: ");
          Serial.print(clients[i].gasLevel);
          Serial.print(" - Alert: ");
          Serial.print(clients[i].gasAlert ? "⚠️  YES" : "✅ NO");
          Serial.print(" - Last update: ");
          Serial.print((millis() - clients[i].lastUpdate) / 1000);
          Serial.println("s ago");
        } else {
          Serial.println("❌ Disconnected");
        }
      }
      Serial.println();
    }
    
    else if (cmd == "help") {
      Serial.println("\n📖 Available commands:");
      Serial.println("  status  - Show device status");
      Serial.println("  help    - Show this message");
      Serial.println();
    }
  }
}
