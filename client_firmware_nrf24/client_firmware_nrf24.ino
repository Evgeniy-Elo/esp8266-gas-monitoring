/*
 * Gas Monitoring System - CLIENT FIRMWARE (NRF24L01+PA+LNA Version)
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, Buzzer, and NRF24L01+PA+LNA
 * 
 * Device IDs: 1-4
 * Wireless communication via 2.4GHz NRF24 radio (longer range)
 */

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <RF24.h>
#include "config.h"

// ============================================
// Client Configuration
// ============================================
#define DEVICE_ID 1  // Change to 1-4 for each client device

// NRF24 Configuration
#define CE_PIN D4    // Chip Enable
#define CSN_PIN D8   // Chip Select (CS)
RF24 radio(CE_PIN, CSN_PIN);

// NRF24 Settings
const byte address[6] = "GASMO";  // Pipe address
const uint16_t SEND_INTERVAL = 500;  // ms between sends

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
  Serial.print("=== Gas Monitor Client (NRF24) - Device ");
  Serial.print(DEVICE_ID);
  Serial.println(" ===\n");
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
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
  radio.setChannel(76);               // Channel 76 (2.476GHz)
  radio.setRetries(15, 15);           // 15 retries, 15*250µs delay
  radio.openWritingPipe(address);
  radio.stopListening();              // Client is transmitter only
  radio.setAutoAck(true);
  
  Serial.println("NRF24 initialized!");
  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);
  Serial.print("PA Level: MAX");
  Serial.println();
  Serial.println("Setup complete!");
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
  
  // Handle buzzer when gas detected
  if (gasDetected) {
    if (now - lastBuzzerToggle >= BUZZER_TONE_DURATION) {
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(BUZZER_PIN, BUZZER_FREQUENCY);
      } else {
        noTone(BUZZER_PIN);
      }
      lastBuzzerToggle = now;
    }
  } else {
    noTone(BUZZER_PIN);
    buzzerState = LOW;
  }
  
  // Send data to server at regular intervals
  if (now - lastDataSend >= SEND_INTERVAL) {
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
    Serial.print("Data sent - Device ");
    Serial.print(DEVICE_ID);
    Serial.print(": ");
    Serial.print(gasLevel);
    Serial.println(" ✓");
  } else {
    Serial.print("Failed to send - Device ");
    Serial.print(DEVICE_ID);
    Serial.println(" ✗");
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
      Serial.print("Device ID: ");
      Serial.println(DEVICE_ID);
      Serial.print("NRF24: ");
      Serial.println(radio.isChipConnected() ? "OK" : "FAILED");
      Serial.print("Gas Level: ");
      Serial.println(gasLevel);
      Serial.print("Gas Detected: ");
      Serial.println(gasDetected ? "YES" : "NO");
      Serial.print("TX Power: MAX");
      Serial.println();
    }
    
    if (cmd == "test") {
      Serial.println("Sending test packet...");
      PayloadData testPayload;
      testPayload.deviceId = DEVICE_ID;
      testPayload.gasLevel = 500;  // High value to trigger alert
      testPayload.timestamp = millis();
      
      if (radio.write(&testPayload, sizeof(testPayload))) {
        Serial.println("Test packet sent successfully!");
      } else {
        Serial.println("Test packet failed!");
      }
    }
  }
}
