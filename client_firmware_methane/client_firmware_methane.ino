/*
 * Gas Monitoring System - CLIENT FIRMWARE (Methane PPM, WiFi)
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, Buzzer
 *
 * Uses MQUnifiedsensor library to calculate LPG (пропан-бутан) concentration in PPM
 * Device IDs: 1-4
 * Connects to server WiFi AP and sends PPM data
 */

#include <ESP8266WiFi.h>
#include <MQUnifiedsensor.h>
#include "config.h"

// ============================================
// Client Configuration
// ============================================
#define DEVICE_ID 1  // Change to 1-4 for each client device

// MQUnifiedsensor configuration (based on Example 3 from know.smartelements.ru)
#define BOARD            ("ESP8266")
#define VOLTAGE_RES       (3.3)
#define ADC_BIT_RES       (10)
#define SENSOR_TYPE       ("MQ-2")
#define RATIO_CLEAN_AIR   (9.83)

// LPG (пропан-бутан) regression values from MQ-2 datasheet
// PPM = A * (RS/R0)^B
#define CH4_A  (574.25)
#define CH4_B  (−2.222)

MQUnifiedsensor MQ2(BOARD, VOLTAGE_RES, ADC_BIT_RES, MQ2_PIN, SENSOR_TYPE);

// WiFi client
WiFiClient client;
bool connected = false;

// Connection attempts tracking
int connectionAttempts = 0;
unsigned long lastConnectionAttempt = 0;

// ============================================
// Sensor and Hardware State
// ============================================
float gasPPM = 0.0;
uint16_t gasLevel = 0;  // PPM value as uint16 for server
bool gasDetected = false;
bool wasGasDetected = false;
bool sensorCalibrated = false;

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
  Serial.print("╔═══════════════════════════════════════╗\n");
  Serial.print("║ Gas Monitor Client LPG (пропан-бутан) ║\n");
  Serial.print("╚═══════════════════════════════════════╝\n\n");

  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);
  Serial.println();

  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

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
  Serial.print("  Methane Threshold: ");
  Serial.print(CH4_THRESHOLD);
  Serial.println(" PPM");
  Serial.println();

  // Initialize MQ-2 sensor with MQUnifiedsensor
  MQ2.setRegressionMethod(1);  // PPM = A * ratio^B
  MQ2.setA(CH4_A);
  MQ2.setB(CH4_B);

  MQ2.init();

  Serial.print("Calibrating MQ-2 for LPG (пропан-бутан)...");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ2.update();
    calcR0 += MQ2.calibrate(RATIO_CLEAN_AIR);
    Serial.print(".");
    delay(500);
  }
  MQ2.setR0(calcR0 / 10);

  if (isinf(calcR0) || calcR0 == 0) {
    Serial.println("\n❌ Calibration failed! Check sensor wiring.");
    Serial.println("   - Verify MQ-2 analog output connected to A0");
    Serial.println("   - Check sensor has 5V power");
    Serial.println("   - Ensure sensor warmed up (1 min minimum)");
    while (1) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }

  sensorCalibrated = true;
  Serial.println(" done!");
  Serial.print("  R0 = ");
  Serial.println(calcR0 / 10);
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
  gasDetected = (gasPPM >= CH4_THRESHOLD);

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
// Sensor Reading (Methane PPM via MQUnifiedsensor)
// ============================================
void readSensor() {
  if (!sensorCalibrated) return;

  MQ2.update();       // Read analog pin
  gasPPM = MQ2.readSensor();  // Calculate PPM via regression model
  gasLevel = (uint16_t)constrain(gasPPM, 0, 65535);
}

// ============================================
// Send Data to Server
// ============================================
void sendDataToServer() {
  if (!connected) {
    return;
  }

  String dataMsg = "DATA:";
  dataMsg += DEVICE_ID;
  dataMsg += ":";
  dataMsg += gasLevel;
  dataMsg += "\n";

  if (client.print(dataMsg)) {
    Serial.print("📤 Device ");
    Serial.print(DEVICE_ID);
    Serial.print(" - LPG: ");
    Serial.print(gasPPM, 1);
    Serial.print(" PPM");
    if (gasDetected) {
      Serial.print("  ⚠️  GAS DETECTED!");
    }
    Serial.println();
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

      Serial.print("Sensor Calibrated: ");
      Serial.println(sensorCalibrated ? "✅ Yes" : "❌ No");

      Serial.print("LPG (пропан-бутан):    ");
      Serial.print(gasPPM, 1);
      Serial.println(" PPM");

      Serial.print("Threshold:        ");
      Serial.print(CH4_THRESHOLD);
      Serial.println(" PPM");

      Serial.print("Gas Detected:     ");
      Serial.println(gasDetected ? "⚠️  YES" : "✅ NO");

      Serial.println();
    }

    else if (cmd == "restart") {
      Serial.println("🔄 Restarting device...");
      delay(1000);
      ESP.restart();
    }

    else if (cmd == "calibrate") {
      Serial.println("🔬 Recalibrating MQ-2 for LPG (пропан-бутан)...");
      float calcR0 = 0;
      for (int i = 1; i <= 10; i++) {
        MQ2.update();
        calcR0 += MQ2.calibrate(RATIO_CLEAN_AIR);
        Serial.print(".");
        delay(500);
      }
      MQ2.setR0(calcR0 / 10);
      sensorCalibrated = true;
      Serial.println(" done!");
      Serial.print("  New R0 = ");
      Serial.println(calcR0 / 10);
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
      Serial.println("  status    - Show device status");
      Serial.println("  calibrate - Recalibrate MQ-2 sensor");
      Serial.println("  restart   - Restart device");
      Serial.println("  scan      - Scan WiFi networks");
      Serial.println("  help      - Show this message");
      Serial.println();
    }
  }
}
