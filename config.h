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
// Server Configuration
// ============================================
#define MAX_CLIENTS 4
// SSD1306 Display (for server only)
#define SSD1306_I2C_ADDR 0x3C
#define I2C_SDA D2          // GPIO4
#define I2C_SCL D1          // GPIO5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DISPLAY_UPDATE_INTERVAL 100  // Faster update for smooth blink
#define STATUS_UPDATE_INTERVAL 1000  // Print status every second

// ============================================
// NRF24L01+PA+LNA Configuration
// ============================================
// SPI pins (hardware):
//   MOSI -> D7 (GPIO13)
//   MISO -> D6 (GPIO12)
//   SCK  -> D5 (GPIO14)
//   GND  -> GND
//   VCC  -> 3.3V (with 10µF capacitor recommended)

#define NRF24_CHANNEL 76        // 2.476 GHz (avoid WiFi channels)
#define NRF24_DATA_RATE 250     // 250 kbps (best range)
#define NRF24_PA_LEVEL 3        // Max power (0=min, 3=max)
#define CE_PIN D4               // Chip Enable
#define CSN_PIN D8              // Chip Select (CS)

// ============================================
// Hardware Client Configuration (Common)
// ============================================
// MQ-2 Sensor
#define MQ2_PIN A0          // Analog input for MQ-2 (ADC) — requires voltage divider 10k+20k from MQ-2 5V output

// LED pins
#define LED_PIN D3          // GPIO0 - warning LED (clients)

// Buzzer pin (for clients only, active buzzer - D0 has no tone/PWM)
#define BUZZER_PIN D0       // GPIO16 - active buzzer


// ============================================
// Gas Threshold
// ============================================
#define GAS_THRESHOLD 300   // MQ-2 threshold value for dangerous concentration (raw ADC)
#define CH4_THRESHOLD 1000  // Methane threshold in PPM (for MQUnifiedsensor version)

// ============================================
// Timing Configuration
// ============================================
#define CONNECTION_RETRY_INTERVAL 5000  // Try to reconnect every 5 seconds
#define CLIENT_TIMEOUT 5000         // ms - time to consider client disconnected
#define SENSOR_READ_INTERVAL 100    // ms - how often to read sensor
#define DATA_SEND_INTERVAL 500      // ms - how often to send data to server
#define LED_BLINK_INTERVAL 200      // ms - LED blink interval when gas detected
#define BUZZER_TONE_DURATION 100    // ms - duration of buzzer tone (toggle interval for active buzzer)

// ============================================
// Device ID (set per client)
// ============================================
// For clients: 1-4
// For server: 0
// This is set in each sketch, not here

#endif // CONFIG_H
