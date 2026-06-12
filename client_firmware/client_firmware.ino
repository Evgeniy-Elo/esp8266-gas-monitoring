/*
 * Gas Monitoring System - CLIENT FIRMWARE
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, and Buzzer
 * 
 * Device IDs: 1-4
 * Connects to server WiFi AP and sends gas concentration data
 */

#include <ESP8266WiFi.h>
#include "config.h"

// ============================================
// Client Configuration
// ============================================
#define DEVICE_ID 1  // Change to 1-4 for each client device

// WiFi client
WiFiClient client;
bool connected = false;

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
  Serial.print("=== Gas Monitor Client - Device ");
  Serial.print(DEVICE_ID);
  Serial.println(" ===\n");
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("Setup complete!");
}

// ============================================
// Main Loop
// ============================================
void loop() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (connected) {
      Serial.println("WiFi connection lost!");
      connected = false;
    }
    connectToWiFi();
  }
  
  // Check if still connected to server
  if (connected && !client.connected()) {
    Serial.println("Server connection lost!");
    client.stop();
    connected = false;
  }
  
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
        // Tone generation using digitalWrite (for PWM simulation)
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
  if (connected && now - lastDataSend >= DATA_SEND_INTERVAL) {
    sendDataToServer();
    lastDataSend = now;
  }
  
  delay(10);
}

// ============================================
// WiFi Connection
// ============================================
void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  
  if (WiFi.status() == WL_IDLE_STATUS) {
    Serial.println("Connecting to WiFi AP...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nWiFi connected! IP: ");
      Serial.println(WiFi.localIP());
      
      // Connect to server
      connectToServer();
    } else {
      Serial.println("\nFailed to connect to WiFi");
    }
  }
}

// ============================================
// Server Connection
// ============================================
void connectToServer() {
  if (connected) {
    return;
  }
  
  Serial.print("Connecting to server at ");
  Serial.print(SERVER_IP_ADDR);
  Serial.print(":");
  Serial.println(SERVER_PORT);
  
  if (client.connect(SERVER_IP_ADDR, SERVER_PORT)) {
    connected = true;
    Serial.println("Connected to server!");
    
    // Send initial registration message
    String regMsg = "REG:";
    regMsg += DEVICE_ID;
    regMsg += "\n";
    client.print(regMsg);
    
  } else {
    Serial.println("Failed to connect to server");
    delay(2000);
  }
}

// ============================================
// Sensor Reading
// ============================================
void readSensor() {
  gasLevel = analogRead(MQ2_PIN);
}

// ============================================
// Send Data to Server
// ============================================
void sendDataToServer() {
  if (!connected) {
    return;
  }
  
  // Send data in format: DATA:device_id:gas_level
  String dataMsg = "DATA:";
  dataMsg += DEVICE_ID;
  dataMsg += ":";
  dataMsg += gasLevel;
  dataMsg += "\n";
  
  if (client.print(dataMsg)) {
    Serial.print("Sent: ");
    Serial.print(dataMsg);
  } else {
    Serial.println("Failed to send data!");
    connected = false;
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
      Serial.print("WiFi: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.print("Server: ");
      Serial.println(connected ? "Connected" : "Disconnected");
      Serial.print("Gas Level: ");
      Serial.println(gasLevel);
      Serial.print("Gas Detected: ");
      Serial.println(gasDetected ? "YES" : "NO");
    }
  }
}
