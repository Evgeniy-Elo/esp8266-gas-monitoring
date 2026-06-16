/*
 * Gas Monitoring System - CLIENT FIRMWARE (NRF24L01+PA+LNA Version)
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, Buzzer, and NRF24L01+PA+LNA
 * 
 * Device IDs: 1-4
 * Wireless communication via 2.4GHz NRF24 radio (longer range)
 */

#include <SPI.h>
#include <RF24.h>
#include "config.h"

// ============================================
// Client Configuration
// ============================================
#define DEVICE_ID 1  // Change to 1-4 for each client device

// NRF24 Configuration
RF24 radio(CE_PIN, CSN_PIN);

// NRF24 Settings
const byte address[6] = "GASMO";  // Pipe address
// Uses DATA_SEND_INTERVAL from config.h

// Payload structure
struct PayloadData {
  uint8_t deviceId;
  uint16_t gasLevel;
  uint32_t timestamp;
};

// ============================================
// Sensor and Hardware State
// ============================================
uint16_t gasLevel = 0;
bool gasDetected = false;
bool wasGasDetected = false;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastLedBlink = 0;
unsigned long lastBuzzerToggle = 0;

// LED and Buzzer state
bool ledState = LOW;
bool buzzerState = LOW;

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.print("╔════════════════════════════════╗\n");
  Serial.print("║   Gas Monitor Client (NRF24)   ║\n");
  Serial.print("╚════════════════════════════════╝\n\n");
  
  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);
  Serial.println();
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Print configuration
  Serial.println("Configuration:");
  Serial.print("  Gas Threshold: ");
  Serial.println(GAS_THRESHOLD);
  Serial.print("  NRF24 Channel: ");
  Serial.println(NRF24_CHANNEL);
  Serial.print("  NRF24 Data Rate: ");
  Serial.print(NRF24_DATA_RATE);
  Serial.println(" kbps");
  Serial.print("  NRF24 PA Level: ");
  Serial.println(NRF24_PA_LEVEL == 3 ? "MAX" : String(NRF24_PA_LEVEL));
  Serial.println();
  
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
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  // Configure NRF24 with PA and LNA enabled
  radio.setPALevel(RF24_PA_MAX);      // Maximum power
  radio.setDataRate(RF24_250KBPS);    // Slower for better range
  radio.setChannel(NRF24_CHANNEL);    // From config.h
  radio.setRetries(15, 15);           // 15 retries, 15*250µs delay
  radio.openWritingPipe(address);
  radio.stopListening();              // Client is transmitter only
  radio.setAutoAck(true);
  
  Serial.print("✅ NRF24 initialized! Device ");
  Serial.print(DEVICE_ID);
  Serial.println(" ready");
  Serial.println();
  Serial.println("Setup complete!");
  Serial.println();
}

// ============================================
// Main Loop
// ============================================
void loop() {
  unsigned long now = millis();
  
  // Read sensor at regular intervals
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensor();
    lastSensorRead = now;
  }
  
  // Detect gas threshold
  wasGasDetected = gasDetected;
  gasDetected = (gasLevel >= GAS_THRESHOLD);
  
  // Handle LED blinking when gas detected
  if (gasDetected) {
    if (now - lastLedBlink >= LED_BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastLedBlink = now;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
  
  // Handle buzzer when gas detected (active buzzer on D0)
  if (gasDetected) {
    if (now - lastBuzzerToggle >= BUZZER_TONE_DURATION) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBuzzerToggle = now;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = LOW;
  }
  
  // Send data to server at regular intervals
  if (now - lastDataSend >= DATA_SEND_INTERVAL) {
    sendDataToServer();
    lastDataSend = now;
  }
  
  delay(10);
}

// ============================================
// Sensor Reading
// ============================================
void readSensor() {
  gasLevel = analogRead(MQ2_PIN);
}

// ============================================
// Send Data to Server via NRF24
// ============================================
void sendDataToServer() {
  PayloadData payload;
  payload.deviceId = DEVICE_ID;
  payload.gasLevel = gasLevel;
  payload.timestamp = millis();
  
  if (radio.write(&payload, sizeof(payload))) {
    Serial.print("📤 Device ");
    Serial.print(DEVICE_ID);
    Serial.print(" - Gas: ");
    Serial.print(gasLevel);
    if (gasDetected) {
      Serial.print("  ⚠️  GAS DETECTED!");
    }
    Serial.println();
  } else {
    Serial.print("❌ Failed to send - Device ");
    Serial.print(DEVICE_ID);
    Serial.println();
  }
}

// ============================================
// Serial Debug Commands
// ============================================
void serialEvent() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "status") {
      Serial.println("\n╔════════════════════════════════╗");
      Serial.println("║       DEVICE STATUS REPORT       ║");
      Serial.println("╚════════════════════════════════╝");
      
      Serial.print("Device ID:        ");
      Serial.println(DEVICE_ID);
      
      Serial.print("NRF24 Radio:      ");
      Serial.print(radio.isChipConnected() ? "✅ Connected" : "❌ Failed");
      Serial.println();
      
      Serial.print("Gas Level:        ");
      Serial.println(gasLevel);
      
      Serial.print("Gas Threshold:    ");
      Serial.println(GAS_THRESHOLD);
      
      Serial.print("Gas Detected:     ");
      Serial.println(gasDetected ? "⚠️  YES" : "✅ NO");
      
      Serial.println();
    }
    
    else if (cmd == "test") {
      Serial.println("📨 Sending test packet...");
      PayloadData testPayload;
      testPayload.deviceId = DEVICE_ID;
      testPayload.gasLevel = 500;  // High value to trigger alert
      testPayload.timestamp = millis();
      
      if (radio.write(&testPayload, sizeof(testPayload))) {
        Serial.println("✅ Test packet sent successfully!");
      } else {
        Serial.println("❌ Test packet failed!");
      }
    }
    
    else if (cmd == "restart") {
      Serial.println("🔄 Restarting device...");
      delay(1000);
      ESP.restart();
    }
    
    else if (cmd == "help") {
      Serial.println("\n📖 Available commands:");
      Serial.println("  status  - Show device status");
      Serial.println("  test    - Send test packet to server");
      Serial.println("  restart - Restart device");
      Serial.println("  help    - Show this message");
      Serial.println();
    }
  }
}
