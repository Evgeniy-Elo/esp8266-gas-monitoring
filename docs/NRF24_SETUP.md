# NRF24L01+PA+LNA Wireless Version Guide

Беспроводная версия системы мониторинга газа с использованием модулей NRF24L01+PA+LNA вместо WiFi.

## 📡 Преимущества NRF24 версии

✅ **Больший радиус действия** - до 1000м в открытом пространстве (с антенной PA+LNA)  
✅ **Меньше помех** - работает на 2.4GHz независимо от WiFi  
✅ **Ниже энергопотребление** - лучше для батарейных устройств  
✅ **Проще** - не требует настройки WiFi сети  
✅ **Надёжнее** - шифрование и повторные отправки встроены  

## 🔌 Подключение NRF24L01+PA+LNA к ESP8266

### Pinout NRF24L01+PA+LNA:

```
┌─────────────────────────────┐
│ NRF24L01+PA+LNA Module      │
├─────────────────────────────┤
│ Pin  │ Name   │ Connect     │
├──────┼────────┼─────────────┤
│  1   │ GND    │ GND         │
│  2   │ VCC    │ 3.3V*       │
│  3   │ CE     │ D4 (GPIO2)  │
│  4   │ CSN    │ D8 (GPIO15) │
│  5   │ SCK    │ D5 (GPIO14) │
│  6   │ MOSI   │ D7 (GPIO13) │
│  7   │ MISO   │ D6 (GPIO12) │
│  8   │ IRQ    │ (не подключ)│
└─────────────────────────────┘

* VCC ДОЛЖНА БЫТЬ 3.3V С КОНДЕНСАТОРОМ 10µF!
```

### Схема подключения:

```
ESP8266 Wemos D1 mini
┌─────────────────────────┐
│  D1  D2  D3  D4  D5     │  <- Top row
│  D6  D7  D8  Gnd 3V3    │  <- Bottom row
└─────────────────────────┘

NRF24L01+PA+LNA
┌──────────────────────┐
│ GND ─────────> GND   │
│ VCC ─────────> 3V3   │
│ CE  ─────────> D4    │
│ CSN ─────────> D8    │
│ SCK ─────────> D5    │
│ MOSI────────> D7    │
│ MISO────────> D6    │
│ IRQ ─────────> (не используется)
└──────────────────────┘

⚠️ ВАЖНО: Подключите конденсатор 10µF параллельно VCC к GND модуля!
```

## 🚀 Установка библиотек

### Arduino IDE:
1. **Sketch → Include Library → Manage Libraries**
2. Найти и установить:
   - `RF24` by TMRh20
   - `Adafruit SSD1306` (для сервера)
   - `Adafruit GFX Library` (для сервера)

### PlatformIO:
```ini
[env:wemos_d1_mini_nrf24]
platform = espressif8266
board = d1_mini
framework = arduino

lib_deps =
    tmrh20/RF24 @ ^1.4.0
    adafruit/Adafruit SSD1306 @ ^2.5.0
    adafruit/Adafruit GFX Library @ ^1.11.0
```

## 📝 Загрузка прошивок (NRF24 версия)

### Для сервера:
1. Откройте `server_firmware_nrf24/server_firmware_nrf24.ino`
2. Выберите плату: **Wemos D1 mini**
3. **Важно**: SPI пины по умолчанию на D5, D6, D7 - проверьте, что не конфликтуют
4. Загрузите на сервер
5. Откройте Serial Monitor (115200 baud)

### Для каждого клиента (4 штуки):

**Выберите тип клиента:**

1. **Raw ADC** (`client_firmware_nrf24/`):
   - Откройте `client_firmware_nrf24.ino`
   - Измерения: 0–1023 (raw)
   - Команды: `status`, `test`, `restart`, `help`

2. **Метан PPM** (`client_firmware_methane_nrf24/`):
   - Откройте `client_firmware_methane_nrf24.ino`
   - Измерения: CH₄ в PPM (MQUnifiedsensor)
   - Требует установки библиотеки `MQUnifiedsensor`
   - Команды: `status`, `test` (шлёт 1500 PPM), `calibrate`, `restart`, `help`
   - **Совместим с тем же сервером** — формат пакета одинаков (uint16_t)

3. **Для обоих типов:**
   ```cpp
   #define DEVICE_ID 1  // Измените на 1, 2, 3 или 4
   ```
   Проверьте подключение NRF24, загрузите, откройте Serial Monitor.

## 📊 Формат данных

Структура пакета (16 байт):
```cpp
struct PayloadData {
  uint8_t deviceId;    // ID устройства (1-4)
  uint16_t gasLevel;   // Значение датчика (0-1023)
  uint32_t timestamp;  // Время отправки (мс)
};
```

**Канал**: 76 (2.476 GHz)  
**Скорость**: 250 kbps (лучший дальнобой)  
**Мощность**: MAX (PA+LNA модуль)  
**Повторные попытки**: 15 x 250µs

### Параметры регрессии MQ-2 (все газы)

Формула: `PPM = A × ratio^B`, `RatioMQ2CleanAir = 9.83` (общий для всех газов).

| Газ | A | B |
|-----|---|---|
| **CH₄ (метан) — по умолчанию** | **447.71** | **−3.245** |
| H₂ (водород) | 987.99 | −2.162 |
| LPG (пропан-бутан) | 574.25 | −2.222 |
| CO (угарный газ) | 36974 | −3.109 |
| Alcohol (этанол) | 3616.1 | −2.675 |
| Propane (пропан) | 658.71 | −2.168 |

Значения взяты из примеров MQSensorsLib и даташита MQ-2. Для смены газа измените `MQ2.setA()` / `MQ2.setB()` в прошивке.

## 🔍 Отладка

### Serial Monitor команды:

**Клиент (raw ADC):**
```
status   - Показать статус радио и газовые данные
test     - Отправить тестовый пакет (gasLevel=500)
restart  - Перезагрузка устройства
help     - Показать список команд
```

**Клиент (метан PPM):**
```
status   - Показать статус радио и газовые данные (PPM)
test     - Отправить тестовый пакет (1500 PPM — выше CH4_THRESHOLD)
calibrate - Повторная калибровка датчика (10 измерений)
restart  - Перезагрузка устройства
help     - Показать список команд
```

**Сервер:**
```
status  - Показать статус всех устройств
rssi    - Показать уровень сигнала
help    - Показать список команд
```

### Пример вывода сервера:
```
> status
╔════════════════════════════════╗
║       SERVER STATUS REPORT       ║
╚════════════════════════════════╝
NRF24 Radio:      ✅ Connected
Connected Clients: 3

Device 1: ✅ Connected - Gas: 245 - Alert: ✅ NO - Last update: 2s ago
Device 2: ✅ Connected - Gas: 312 - Alert: ⚠️  YES - Last update: 1s ago
Device 3: ❌ Disconnected
Device 4: ✅ Connected - Gas: 189 - Alert: ✅ NO - Last update: 5s ago
```

### Пример вывода клиента (raw ADC):
```
╔════════════════════════════════╗
║   Gas Monitor Client (NRF24)   ║
╚════════════════════════════════╝

Device ID: 1

Configuration:
  Gas Threshold: 300
  NRF24 Channel: 76
  NRF24 Data Rate: 250 kbps
  NRF24 PA Level: MAX

✅ NRF24 initialized! Device 1 ready

📤 Device 1 - Gas: 245
📤 Device 1 - Gas: 267
📤 Device 1 - Gas: 310  ⚠️  GAS DETECTED!
❌ Failed to send - Device 1
```

### Пример вывода клиента (метан PPM):
```
╔════════════════════════════════╗
║ Gas Monitor Client CH4 (NRF24) ║
╚════════════════════════════════╝

Device ID: 2

Configuration:
  Gas Threshold: 1000 PPM
  CH4_A: 447.71, CH4_B: -3.245
  RatioMQ2CleanAir: 9.83
  NRF24 Channel: 76
  NRF24 Data Rate: 250 kbps
  NRF24 PA Level: MAX

✅ Calibration complete (10 readings): R0 = 2.45
✅ NRF24 initialized! Device 2 ready

📤 Device 2 - Gas: 450 PPM
📤 Device 2 - Gas: 320 PPM
📤 Device 2 - Gas: 1200 PPM  ⚠️  GAS DETECTED!
❌ Failed to send - Device 2
```

## ⚠️ Возможные проблемы

### NRF24 не инициализируется:
```
❌ NRF24 initialization failed!
   Check wiring:
   CE  -> D4 (GPIO2)
   CSN -> D8 (GPIO15)
   MOSI -> D7 (GPIO13)
   MISO -> D6 (GPIO12)
   SCK  -> D5 (GPIO14)
   GND  -> GND
   VCC  -> 3.3V with 10µF capacitor
```

**Решение:**
- Проверьте все провода
- Убедитесь в конденсаторе на VCC
- Попробуйте SPI scanner из примеров Arduino
- Проверьте, что SPI не конфликтует с другими устройствами

### Сервер не видит клиентов:
- Убедитесь, что оба модуля на канале 76
- Проверьте адрес "GASMO" совпадает на сервере и клиентах
- Убедитесь, что `NRF24_PA_LEVEL` в config.h установлен в 3 (макс. мощность)
- Уменьшите расстояние для теста

### Нестабильная связь:
- Уменьшите скорость с 250 kbps до 1 Mbps (если нужна дальность)
- Добавьте дополнительный конденсатор 100nF близко к VCC модуля
- Используйте кабели покороче (< 10см)
- Используйте ферритовое кольцо на питающем проводе

### LED мигают, но нет данных:
- Клиент может быть в режиме transmit-only (это нормально для клиента)
- Сервер должен быть в режиме listen
- Проверьте адрес трубы `address[6] = "GASMO"`

## 🔧 Оптимизация дальности

Для максимальной дальности:

1. **Используйте PA+LNA модули** (у вас уже есть) ✓
2. **Внешняя антенна** - припаяйте SMA разъём к модулю
3. **Направленная антенна** - для точечной передачи
4. **Уменьшите частоту передачи** - 250 kbps вместо 2 Mbps
5. **Добавьте ретрансляцию** - одна из плат как ретранслятор
6. **Экранирование** - минимизируйте помехи

## 📡 Дальность типичная:

| Конфигурация | Расстояние |
|-------------|-----------|
| Базовый NRF24 | 50-100м |
| NRF24+PA+LNA | 200-500м |
| +внешняя антенна | 500-1000м |
| +направленная | 1000-2000м |

(В открытом пространстве без препятствий)

## 🔐 Безопасность

Текущая версия использует открытый канал на `address = "GASMO"`.

Для добавления безопасности:
- Добавить шифрование (XOR простое или AES)
- Использовать другой адрес вместо "GASMO"
- Добавить CRC проверку пакетов

## 📚 Полезные ссылки

- [RF24 библиотека](https://github.com/tmrh20/RF24)
- [NRF24L01+ документация](https://www.nordicsemi.com/Products/Low-power-short-range-wireless/nRF24L01-plus)
- [Datasheet](https://www.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01_Product_Specification_v2_0.pdf)

