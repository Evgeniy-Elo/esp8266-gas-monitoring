/*
 * Gas Monitoring System - Configuration Header
 * ESP8266 Wemos D1 mini
 * 
 * Supports both WiFi TCP/IP and NRF24L01+PA+LNA wireless versions
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration (for TCP/IP version)
// ============================================
#define WIFI_SSID "GAS_MONITOR_AP"
#define WIFI_PASSWORD "12345678"

// Server configuration (fixed IP for stability)
#define SERVER_IP_ADDR IPAddress(192, 168, 4, 1)
#define SERVER_PORT 8888

// ============================================
// Hardware Configuration (Common)
// ============================================
// MQ-2 Sensor
#define MQ2_PIN A0          // Analog input for MQ-2 (ADC)

// LED pins
#define LED_PIN D8          // GPIO15 - warning LED (clients)

// Buzzer pin (for clients only)
#define BUZZER_PIN D7       // GPIO13 - piezo buzzer

// SSD1306 Display (for server only)
#define SSD1306_I2C_ADDR 0x3C
#define I2C_SDA D2          // GPIO4
#define I2C_SCL D1          // GPIO5

// ============================================
// NRF24L01+PA+LNA Configuration
// ============================================
// CE_PIN and CSN_PIN are defined in each firmware file
// SPI pins (hardware):
//   MOSI -> D7 (GPIO13)
//   MISO -> D6 (GPIO12) 
//   SCK  -> D5 (GPIO14)
//   GND  -> GND
//   VCC  -> 3.3V (with 10µF capacitor recommended)

#define NRF24_CHANNEL 76        // 2.476 GHz (avoid WiFi channels)
#define NRF24_DATA_RATE 250     // 250 kbps (best range)
#define NRF24_PA_LEVEL 3        // Max power (0=min, 3=max)

// ============================================
// Gas Threshold
// ============================================
#define GAS_THRESHOLD 300   // MQ-2 threshold value for dangerous concentration

// ============================================
// Timing Configuration
// ============================================
#define SENSOR_READ_INTERVAL 100    // ms - how often to read sensor
#define DATA_SEND_INTERVAL 500      // ms - how often to send data to server
#define LED_BLINK_INTERVAL 200      // ms - LED blink interval when gas detected
#define BUZZER_TONE_DURATION 100    // ms - duration of buzzer tone
#define BUZZER_FREQUENCY 1000       // Hz - buzzer frequency

// ============================================
// Device ID (set per client)
// ============================================
// For clients: 1-4
// For server: 0
// This is set in each sketch, not here

#endif // CONFIG_H
