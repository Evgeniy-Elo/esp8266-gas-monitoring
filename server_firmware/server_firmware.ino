/*
 * Gas Monitoring System - SERVER FIRMWARE
 * ESP8266 Wemos D1 mini with SSD1306 Display and 4 LEDs
 * 
 * Acts as WiFi Access Point and displays data from all 4 client devices
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
};

ClientData clients[MAX_CLIENTS] = {
  {0, false, false, 0, 0, false},
  {0, false, false, 0, 0, false},
  {0, false, false, 0, 0, false},
  {0, false, false, 0, 0, false}
};

// Timing
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 500

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("=== Gas Monitor SERVER ===\n");
  
  // Initialize I2C and Display
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while(1);
  }
  
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
  
  // Start WiFi AP
  setupWiFiAP();
  
  // Start server
  wifiServer.begin();
  Serial.println("WiFi server started!");
  
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
  
  // Update LED states
  updateLEDs(now);
  
  // Update display
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  delay(10);
}

// ============================================
// WiFi AP Setup
// ============================================
void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  WiFi.softAPConfig(SERVER_IP_ADDR, SERVER_IP_ADDR, IPAddress(255, 255, 255, 0));
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

// ============================================
// Handle New Client Connections
// ============================================
void handleNewConnections() {
  WiFiClient newClient = wifiServer.available();
  
  if (newClient) {
    Serial.println("New client connection!");
    
    // Find empty slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!serverClients[i].connected()) {
        serverClients[i] = newClient;
        clients[i].connected = true;
        clients[i].lastUpdate = millis();
        Serial.print("Client assigned to slot ");
        Serial.println(i + 1);
        
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
              
              Serial.print("Device ");
              Serial.print(deviceId);
              Serial.print(" - Gas Level: ");
              Serial.print(gasLevel);
              Serial.print(" - Alert: ");
              Serial.println(clients[idx].gasAlert ? "YES" : "NO");
            }
          }
        }
      }
    } else {
      // Client disconnected
      if (clients[i].connected) {
        Serial.print("Client ");
        Serial.print(i + 1);
        Serial.println(" disconnected!");
        
        clients[i].connected = false;
        clients[i].gasAlert = false;
        
        // Turn off LED for this device
        updateDeviceLED(i, false, false);
      }
      
      // Check for stale connections
      if (millis() - clients[i].lastUpdate > 5000) {
        clients[i].connected = false;
      }
    }
  }
}

// ============================================
// Update Display
// ============================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // Header
  display.println("=== Gas Monitoring ===");
  display.println();
  
  // Device status
  for (int i = 0; i < MAX_CLIENTS; i++) {
    display.print("Device ");
    display.print(i + 1);
    display.print(": ");
    
    if (clients[i].connected) {
      display.print(clients[i].gasLevel);
      
      if (clients[i].gasAlert) {
        display.print(" [ALERT!]");
      }
    } else {
      display.print("--");
    }
    
    display.println();
  }
  
  // Display active alerts count
  int alertCount = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].gasAlert) {
      alertCount++;
    }
  }
  
  display.println();
  if (alertCount > 0) {
    display.print("ALERTS: ");
    display.println(alertCount);
  } else {
    display.println("Status: Normal");
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
      // Blink LED if gas detected
      if (now - clients[i].lastBlinkToggle >= LED_BLINK_INTERVAL) {
        clients[i].ledState = !clients[i].ledState;
        digitalWrite(ledPins[i], clients[i].ledState ? HIGH : LOW);
        clients[i].lastBlinkToggle = now;
      }
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
// Serial Debug
// ============================================
void serialEvent() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "status") {
      Serial.println("\n=== Server Status ===");
      Serial.print("WiFi AP IP: ");
      Serial.println(WiFi.softAPIP());
      Serial.print("Connected Clients: ");
      int connectedCount = 0;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) connectedCount++;
      }
      Serial.println(connectedCount);
      
      for (int i = 0; i < MAX_CLIENTS; i++) {
        Serial.print("Device ");
        Serial.print(i + 1);
        Serial.print(": ");
        if (clients[i].connected) {
          Serial.print("Connected - Gas: ");
          Serial.print(clients[i].gasLevel);
          Serial.print(" - Alert: ");
          Serial.println(clients[i].gasAlert ? "YES" : "NO");
        } else {
          Serial.println("Disconnected");
        }
      }
      Serial.println();
    }
  }
}
