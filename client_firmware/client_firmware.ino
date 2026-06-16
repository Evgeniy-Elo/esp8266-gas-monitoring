/*
 * Gas Monitoring System - CLIENT FIRMWARE (IMPROVED)
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, and Buzzer
 * 
 * Device IDs: 1-4
 * Connects to server WiFi AP and sends gas concentration data
 * 
 * IMPROVED: Better diagnostics, auto-reconnect, debug output
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

// Connection attempts tracking
int connectionAttempts = 0;
unsigned long lastConnectionAttempt = 0;

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
  
  // Print configuration
  Serial.println("Configuration:");
  Serial.print("  SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("  Server: ");
  Serial.print(SERVER_IP_ADDR);
  Serial.print(":");
  Serial.println(SERVER_PORT);
  Serial.print("  Gas Threshold: ");
  Serial.println(GAS_THRESHOLD);
  Serial.println();
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("Setup complete!");
  Serial.println();
}

// ============================================
// Main Loop
// ============================================
void loop() {
  unsigned long now = millis();
  
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (connected) {
      Serial.println("⚠️  WiFi connection lost!");
      connected = false;
    }
    
    // Try to reconnect periodically
    if (now - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
      connectToWiFi();
      lastConnectionAttempt = now;
    }
  }
  
  // Check if server connection is still alive
  if (WiFi.status() == WL_CONNECTED && connected && !client.connected()) {
    Serial.println("⚠️  Server connection lost!");
    client.stop();
    connected = false;
  }
  
  // Try to connect to server if WiFi is connected but no server connection
  if (WiFi.status() == WL_CONNECTED && !connected) {
    if (now - lastConnectionAttempt >= CONNECTION_RETRY_INTERVAL) {
      connectToServer();
      lastConnectionAttempt = now;
    }
  }
  
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
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBuzzerToggle = now;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = LOW;
  }
  
  // Send data to server at regular intervals
  if (connected && now - lastDataSend >= DATA_SEND_INTERVAL) {
    sendDataToServer();
    lastDataSend = now;
  }
  
  // Check for incoming data from server (optional)
  if (client.available()) {
    String response = client.readStringUntil('\n');
    Serial.print("Server response: ");
    Serial.println(response);
  }
  
  // Handle serial input
  serialEvent();
  
  delay(10);
}

// ============================================
// WiFi Connection
// ============================================
void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  
  Serial.print("📡 Connecting to WiFi AP: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi connected!");
    Serial.print("📍 Local IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("📡 Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println();
    
    // Try to connect to server
    connectToServer();
  } else {
    Serial.println("❌ Failed to connect to WiFi");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
  }
}

// ============================================
// Server Connection
// ============================================
void connectToServer() {
  if (connected) {
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Cannot connect to server - WiFi not connected");
    return;
  }
  
  Serial.print("🔗 Connecting to server: ");
  Serial.print(SERVER_IP_ADDR);
  Serial.print(":");
  Serial.println(SERVER_PORT);
  
  if (client.connect(SERVER_IP_ADDR, SERVER_PORT)) {
    connected = true;
    Serial.println("✅ Connected to server!");
    Serial.println();
    
    // Send initial registration message
    String regMsg = "REG:";
    regMsg += DEVICE_ID;
    regMsg += "\n";
    client.print(regMsg);
    
    Serial.print("📨 Sent registration: REG:");
    Serial.println(DEVICE_ID);
    Serial.println();
    
  } else {
    Serial.println("❌ Failed to connect to server");
    Serial.println("   - Check if server is running");
    Serial.println("   - Check network connectivity");
    Serial.println("   - Verify SERVER_IP_ADDR and SERVER_PORT in config.h");
    Serial.println();
    connected = false;
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
    Serial.print("📤 Sent: ");
    Serial.print(dataMsg);
    if (gasDetected) {
      Serial.println("  ⚠️  GAS DETECTED!");
    }
  } else {
    Serial.println("❌ Failed to send data!");
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
      Serial.println("\n╔════════════════════════════════╗");
      Serial.println("║       DEVICE STATUS REPORT       ║");
      Serial.println("╚════════════════════════════════╝");
      
      Serial.print("Device ID:        ");
      Serial.println(DEVICE_ID);
      
      Serial.print("WiFi Status:      ");
      Serial.print(WiFi.status() == WL_CONNECTED ? "✅ Connected" : "❌ Disconnected");
      Serial.println();
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Local IP:         ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal Strength:  ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
      }
      
      Serial.print("Server Status:    ");
      Serial.print(connected ? "✅ Connected" : "❌ Disconnected");
      Serial.println();
      
      Serial.print("Gas Level:        ");
      Serial.println(gasLevel);
      
      Serial.print("Gas Threshold:    ");
      Serial.println(GAS_THRESHOLD);
      
      Serial.print("Gas Detected:     ");
      Serial.println(gasDetected ? "⚠️  YES" : "✅ NO");
      
      Serial.println();
    }
    
    else if (cmd == "restart") {
      Serial.println("🔄 Restarting device...");
      delay(1000);
      ESP.restart();
    }
    
    else if (cmd == "scan") {
      Serial.println("🔍 Scanning for WiFi networks...");
      int networks = WiFi.scanNetworks();
      Serial.print("Found ");
      Serial.print(networks);
      Serial.println(" networks:");
      
      for (int i = 0; i < networks; i++) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm)");
      }
      Serial.println();
    }
    
    else if (cmd == "help") {
      Serial.println("\n📖 Available commands:");
      Serial.println("  status  - Show device status");
      Serial.println("  restart - Restart device");
      Serial.println("  scan    - Scan WiFi networks");
      Serial.println("  help    - Show this message");
      Serial.println();
    }
  }
}
