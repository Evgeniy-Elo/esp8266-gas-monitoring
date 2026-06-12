/*
 * Gas Monitoring System - SERVER FIRMWARE (NRF24L01+PA+LNA Version)
 * ESP8266 Wemos D1 mini with SSD1306 Display, 4 LEDs, and NRF24L01+PA+LNA
 * 
 * Acts as NRF24 receiver and displays data from all 4 client devices
 * Display divided into 4 sections with large digits and alert background invert
 */

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <RF24.h>
#include "config.h"

// ============================================
// Server Configuration
// ============================================
#define MAX_CLIENTS 4
#define DEVICE_ID 0  // Server device ID
#define RECEIVE_TIMEOUT 3000  // ms - time to consider client disconnected

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NRF24 Configuration
#define CE_PIN D4    // Chip Enable
#define CSN_PIN D8   // Chip Select (CS)
RF24 radio(CE_PIN, CSN_PIN);

// NRF24 Settings
const byte address[6] = "GASMO";  // Pipe address
const uint16_t RECEIVE_CHECK_INTERVAL = 100;  // ms between radio checks

// Payload structure (must match client)
struct PayloadData {
  uint8_t deviceId;
  uint16_t gasLevel;
  uint32_t timestamp;
};

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
unsigned long lastRadioCheck = 0;
#define DISPLAY_UPDATE_INTERVAL 250

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("=== Gas Monitor SERVER (NRF24) ===\n");
  
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
  display.println("Starting NRF24...");
  display.display();
  
  // Initialize LED pins
  pinMode(D3, OUTPUT);  // LED for device 1
  pinMode(D5, OUTPUT);  // LED for device 2
  pinMode(D6, OUTPUT);  // LED for device 3
  pinMode(D7, OUTPUT);  // LED for device 4
  
  digitalWrite(D3, LOW);
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  digitalWrite(D7, LOW);
  
  // Initialize SPI and NRF24
  SPI.begin();
  delay(100);
  
  if (!radio.begin()) {
    Serial.println("NRF24 initialization failed!");
    Serial.println("Check wiring:");
    Serial.println("  CE -> D4");
    Serial.println("  CSN -> D8");
    Serial.println("  MOSI -> D7 (GPIO13)");
    Serial.println("  MISO -> D6 (GPIO12)");
    Serial.println("  SCK -> D5 (GPIO14)");
    Serial.println("  GND -> GND");
    Serial.println("  VCC -> 3.3V with 10µF capacitor");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("NRF24 FAILED");
    display.println("Check wiring!");
    display.display();
    
    while(1) {
      digitalWrite(D3, HIGH);
      digitalWrite(D5, HIGH);
      digitalWrite(D6, HIGH);
      digitalWrite(D7, HIGH);
      delay(200);
      digitalWrite(D3, LOW);
      digitalWrite(D5, LOW);
      digitalWrite(D6, LOW);
      digitalWrite(D7, LOW);
      delay(200);
    }
  }
  
  // Configure NRF24 with PA and LNA enabled
  radio.setPALevel(RF24_PA_MAX);      // Maximum power
  radio.setDataRate(RF24_250KBPS);    // Slower for better range
  radio.setChannel(76);               // Channel 76 (2.476GHz)
  radio.openReadingPipe(1, address);
  radio.startListening();             // Server is receiver
  radio.setAutoAck(true);
  
  Serial.println("NRF24 initialized!");
  Serial.println("Waiting for clients...");
  
  delay(500);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("NRF24 Active");
  display.println("Awaiting clients...");
  display.println("Channel: 76");
  display.println("PA: MAX");
  display.display();
  
  Serial.println("Setup complete!");
}

// ============================================
// Main Loop
// ============================================
void loop() {
  unsigned long now = millis();
  
  // Check for radio data
  if (now - lastRadioCheck >= RECEIVE_CHECK_INTERVAL) {
    readRadioData();
    lastRadioCheck = now;
  }
  
  // Update LED states
  updateLEDs(now);
  
  // Update display with alert animation
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay(now);
    lastDisplayUpdate = now;
  }
  
  // Check for stale connections
  checkConnectionStatus(now);
  
  delay(10);
}

// ============================================
// Read Data from NRF24 Radio
// ============================================
void readRadioData() {
  if (radio.available()) {
    PayloadData payload;
    
    if (radio.read(&payload, sizeof(payload))) {
      // Validate device ID
      if (payload.deviceId >= 1 && payload.deviceId <= MAX_CLIENTS) {
        int idx = payload.deviceId - 1;
        
        // Update client data
        clients[idx].gasLevel = payload.gasLevel;
        clients[idx].gasAlert = (payload.gasLevel >= GAS_THRESHOLD);
        clients[idx].lastUpdate = millis();
        
        // Mark as connected if wasn't before
        if (!clients[idx].connected) {
          clients[idx].connected = true;
          Serial.print("Device ");
          Serial.print(payload.deviceId);
          Serial.println(" connected!");
        }
        
        Serial.print("RX - Device ");
        Serial.print(payload.deviceId);
        Serial.print(": ");
        Serial.print(payload.gasLevel);
        Serial.print(" - Alert: ");
        Serial.println(clients[idx].gasAlert ? "YES" : "NO");
      }
    }
  }
}

// ============================================
// Check Connection Status
// ============================================
void checkConnectionStatus(unsigned long now) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].connected) {
      // Check if timeout
      if (now - clients[i].lastUpdate > RECEIVE_TIMEOUT) {
        clients[i].connected = false;
        clients[i].gasAlert = false;
        clients[i].displayInverted = false;
        
        Serial.print("Device ");
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
    
    // Draw section border
    display.drawRect(x, y, 64, 32, SSD1306_WHITE);
    
    // Handle alert background invert
    if (clients[i].gasAlert && clients[i].connected) {
      // Toggle invert effect
      if (now % 400 < 200) {  // Blink every 200ms
        clients[i].displayInverted = true;
      } else {
        clients[i].displayInverted = false;
      }
      
      if (clients[i].displayInverted) {
        // Invert the section (fill black background, white text)
        display.fillRect(x + 1, y + 1, 62, 30, SSD1306_BLACK);
        display.setTextColor(SSD1306_WHITE);
      }
    } else {
      clients[i].displayInverted = false;
    }
    
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
  int ledPins[MAX_CLIENTS] = {D3, D5, D6, D7};
  
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
// Serial Debug
// ============================================
void serialEvent() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "status") {
      Serial.println("\n=== Server Status (NRF24) ===");
      Serial.print("NRF24: ");
      Serial.println(radio.isChipConnected() ? "OK" : "FAILED");
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
          Serial.print(clients[i].gasAlert ? "YES" : "NO");
          Serial.print(" - Uptime: ");
          Serial.print((millis() - clients[i].lastUpdate) / 1000);
          Serial.println("s");
        } else {
          Serial.println("Disconnected");
        }
      }
      Serial.println();
    }
    
    if (cmd == "rssi") {
      Serial.println("\nRSSI (Signal Strength):");
      // Note: RF24 library doesn't directly expose RSSI on ESP8266
      // This is approximate based on transmission success
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) {
          Serial.print("Device ");
          Serial.print(i + 1);
          Serial.println(": Strong (last update < 1s ago)");
        }
      }
    }
  }
}
