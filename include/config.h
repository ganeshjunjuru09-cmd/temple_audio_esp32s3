#pragma once

// IMPORTANT: Confirm the exact ESP32-S3 module/board before flashing.
// This project targets ESP32-S3 N16R8 (16 MB flash + 8 MB PSRAM).

// Keep hardware modules disabled until you wire them and set the GPIOs below.
#define ENABLE_SD          0
#define ENABLE_RTC         0
#define ENABLE_AUDIO       0
#define ENABLE_BLE         1

// Wi-Fi station credentials. Replace these before uploading.
// Do not publish this file with real credentials.
#define WIFI_SSID          "YOUR_WIFI_NAME"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"

// Secure device-created access point used when switching to AP mode.
// Change this to a unique password of at least 12 characters.
#define AP_SSID            "Temple-Audio-Setup"
#define AP_PASSWORD        "ChangeThis1234"

// HTTP dashboard/API authentication. Change this before using on a real network.
#define DASH_USER          "admin"
#define DASH_PASSWORD      "ChangeThisAdmin123"

// ---- GPIO CONFIGURATION ----
// These are intentionally NOT guessed. Set them to the actual wiring.
// The TFT and its onboard microSD may share SPI; SD_CS must be its separate CS pin.
#define SD_CS_PIN          -1
#define SPI_SCK_PIN        -1
#define SPI_MISO_PIN       -1
#define SPI_MOSI_PIN       -1

#define I2C_SDA_PIN        -1
#define I2C_SCL_PIN        -1

#define I2S_BCLK_PIN       -1
#define I2S_LRC_PIN        -1
#define I2S_DOUT_PIN       -1

// Audio library volume range is 0..21.
#define DEFAULT_VOLUME     12

// Timezone for India. Change if deploying outside India.
#define TZ_INFO            "IST-5:30"

// BLE service UUIDs (custom device-control service).
#define BLE_SERVICE_UUID   "8f3a0001-6b7c-4a1d-9e2f-123456789001"
#define BLE_COMMAND_UUID   "8f3a0002-6b7c-4a1d-9e2f-123456789001"
#define BLE_STATUS_UUID    "8f3a0003-6b7c-4a1d-9e2f-123456789001"
