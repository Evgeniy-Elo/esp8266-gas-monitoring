/*
 * Gas Monitoring System - CLIENT FIRMWARE (Methane PPM, NRF24L01+PA+LNA)
 * ESP8266 Wemos D1 mini with MQ-2 Sensor, LED, Buzzer, NRF24L01+PA+LNA
 *
 * Uses MQUnifiedsensor library to calculate LPG (пропан-бутан) concentration in PPM
 * Device IDs: 1-4
 * Wireless communication via 2.4GHz NRF24 radio
 */

#include <SPI.h>
#include <RF24.h>
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
float gasPPM = 0.0;
uint16_t gasLevel = 0;  // PPM value as uint16
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
  Serial.print("║              NRF24 Version            ║\n");
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
  Serial.print("  LPG (пропан-бутан) Threshold: ");
  Serial.print(CH4_THRESHOLD);
  Serial.println(" PPM");
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

  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(NRF24_CHANNEL);
  radio.setRetries(15, 15);
  radio.openWritingPipe(address);
  radio.stopListening();
  radio.setAutoAck(true);

  Serial.print("✅ NRF24 initialized! Device ");
  Serial.print(DEVICE_ID);
  Serial.println(" ready");
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
// Sensor Reading (Methane PPM via MQUnifiedsensor)
// ============================================
void readSensor() {
  if (!sensorCalibrated) return;

  MQ2.update();       // Read analog pin
  gasPPM = MQ2.readSensor();  // Calculate PPM via regression model
  gasLevel = (uint16_t)constrain(gasPPM, 0, 65535);
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
    Serial.print(" - LPG: ");
    Serial.print(gasPPM, 1);
    Serial.print(" PPM");
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

    else if (cmd == "test") {
      Serial.println("📨 Sending test packet...");
      PayloadData testPayload;
      testPayload.deviceId = DEVICE_ID;
      testPayload.gasLevel = 1500;  // Above CH4_THRESHOLD (1000 PPM)
      testPayload.timestamp = millis();

      if (radio.write(&testPayload, sizeof(testPayload))) {
        Serial.println("✅ Test packet sent successfully!");
      } else {
        Serial.println("❌ Test packet failed!");
      }
    }

    else if (cmd == "calibrate") {
      Serial.println("🔬 Recalibrating MQ-2 for methane...");
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

    else if (cmd == "restart") {
      Serial.println("🔄 Restarting device...");
      delay(1000);
      ESP.restart();
    }

    else if (cmd == "help") {
      Serial.println("\n📖 Available commands:");
      Serial.println("  status    - Show device status");
      Serial.println("  test      - Send test packet to server");
      Serial.println("  calibrate - Recalibrate MQ-2 sensor");
      Serial.println("  restart   - Restart device");
      Serial.println("  help      - Show this message");
      Serial.println();
    }
  }
}
