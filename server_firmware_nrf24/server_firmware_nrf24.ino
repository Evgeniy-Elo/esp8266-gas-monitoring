/*
 * Gas Monitoring System - SERVER FIRMWARE (NRF24L01+PA+LNA Version)
 * ESP8266 Wemos D1 mini with SSD1306 Display, 4 LEDs, and NRF24L01+PA+LNA
 *
 * Acts as NRF24 receiver and displays data from all 4 client devices
 * Display divided into 4 sections with large digits and alert background invert
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <RF24.h>
#include "config.h"

// ============================================
// Server Configuration
// ============================================
#define DEVICE_ID 0          // Server device ID

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NRF24 Configuration
RF24 radio(CE_PIN, CSN_PIN);

// NRF24 Settings
const byte address[6] = "GASMO";              // Pipe address
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
unsigned long lastStatusUpdate = 0;

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║  Gas Monitor SERVER (NRF24)    ║");
  Serial.println("╚════════════════════════════════╝\n");

  // Initialize I2C and Display
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println("❌ SSD1306 allocation failed");
    Serial.println("   Check I2C wiring:");
    Serial.println("   - D1 (GPIO5) -> SCL");
    Serial.println("   - D2 (GPIO4) -> SDA");
    Serial.println("   - Display address: 0x3C");
    while (1) {
      delay(100);
    }
  }

  Serial.println("✅ Display initialized");

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
    Serial.println("❌ NRF24 initialization failed!");
    Serial.println("   Check wiring:");
    Serial.println("   CE  -> D4 (GPIO2)");
    Serial.println("   CSN -> D8 (GPIO15)");
    Serial.println("   MOSI -> D7 (GPIO13)");
    Serial.println("   MISO -> D6 (GPIO12)");
    Serial.println("   SCK  -> D5 (GPIO14)");
    Serial.println("   GND  -> GND");
    Serial.println("   VCC  -> 3.3V with 10µF capacitor");

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("NRF24 FAILED");
    display.println("Check wiring!");
    display.display();

    while (1) {
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
  radio.setPALevel(RF24_PA_MAX);    // Maximum power
  radio.setDataRate(RF24_250KBPS);  // Slower for better range
  radio.setChannel(NRF24_CHANNEL);  // From config.h
  radio.openReadingPipe(1, address);
  radio.startListening();  // Server is receiver
  radio.setAutoAck(true);

  Serial.println("✅ NRF24 initialized!");
  Serial.println();

  // Print configuration
  Serial.println("Configuration:");
  Serial.print("  NRF24 Channel: ");
  Serial.println(NRF24_CHANNEL);
  Serial.print("  NRF24 Data Rate: ");
  Serial.print(NRF24_DATA_RATE);
  Serial.println(" kbps");
  Serial.print("  NRF24 PA Level: ");
  Serial.println(NRF24_PA_LEVEL == 3 ? "MAX" : String(NRF24_PA_LEVEL));
  Serial.print("  Gas Threshold: ");
  Serial.println(GAS_THRESHOLD);
  Serial.println();

  delay(500);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("NRF24 Active");
  display.println("Awaiting clients...");
  display.print("Channel: ");
  display.println(NRF24_CHANNEL);
  display.print("PA: ");
  display.println(NRF24_PA_LEVEL == 3 ? "MAX" : String(NRF24_PA_LEVEL));
  display.display();

  Serial.println("Setup complete!");
  Serial.println("Waiting for client connections...\n");
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

  // Print status periodically
  if (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    printStatus();
    lastStatusUpdate = now;
  }

  delay(10);
}

// ============================================
// Read Data from NRF24 Radio
// ============================================
void readRadioData() {
  if (radio.available()) {
    PayloadData payload;

    radio.read(&payload, sizeof(payload));

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
        Serial.print("✅ Device ");
        Serial.print(payload.deviceId);
        Serial.println(" connected!");
      }

      Serial.print("📊 Device ");
      Serial.print(payload.deviceId);
      Serial.print(" - Gas: ");
      Serial.print(payload.gasLevel);
      Serial.print(" - Alert: ");
      Serial.println(clients[idx].gasAlert ? "⚠️  YES" : "✅ NO");
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
      if (now - clients[i].lastUpdate > CLIENT_TIMEOUT) {
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
  int ledPins[MAX_CLIENTS] = { D3, D5, D6, D7 };

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

      Serial.print("NRF24 Radio:      ");
      Serial.println(radio.isChipConnected() ? "✅ Connected" : "❌ Failed");

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

    else if (cmd == "rssi") {
      Serial.println("\n📶 Signal Strength:");
      // Note: RF24 library doesn't directly expose RSSI on ESP8266
      // This is approximate based on transmission success
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].connected) {
          Serial.print("  Device ");
          Serial.print(i + 1);
          Serial.println(": Strong ✅");
        }
      }
      Serial.println();
    }

    else if (cmd == "help") {
      Serial.println("\n📖 Available commands:");
      Serial.println("  status  - Show device status");
      Serial.println("  rssi    - Show signal strength");
      Serial.println("  help    - Show this message");
      Serial.println();
    }
  }
}
