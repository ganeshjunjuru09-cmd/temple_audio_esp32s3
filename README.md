# Temple Audio Automation – ESP32-S3 N16R8

This is a first firmware baseline for the ESP32-S3 N16R8 project. It is intentionally
configured to compile and run the Wi-Fi dashboard + BLE mode before the external
modules are connected. SD, RTC and audio support are present behind feature flags.

## 1. Important safety / reality checks

- This firmware does not make audio possible with the ESP32 board alone. MP3 playback
  requires microSD storage and an I2S DAC/amplifier/speaker path.
- The PCM5102A is a DAC, not a speaker amplifier. Connect its line output to a suitable
  amplifier or powered speaker. Do not connect a passive speaker directly to PCM5102A.
- ESP32-S3 supports BLE, not Bluetooth Classic A2DP speaker streaming.
- AP mode creates a protected Wi-Fi hotspot; it is not Bluetooth. BLE mode turns Wi-Fi off.
- Exact GPIOs cannot safely be guessed. Set the pin constants only after identifying
  the actual board and display/SD module pinout.
- The firmware is a development baseline, not a certified life-safety or public-address
  controller. Bench-test power-loss recovery, volume, schedule edge cases and audio
  before installing it in a temple.

## 2. Install

1. Install VS Code + PlatformIO.
2. Open this folder as a PlatformIO project.
3. Confirm the board is actually ESP32-S3 N16R8. The supplied PlatformIO board target
   is a generic ESP32-S3 DevKitC-1 target; if your board has a different USB/flash
   configuration, select its exact board definition.
4. Edit `include/config.h`:
   - Set Wi-Fi credentials.
   - Change AP password and dashboard password.
   - Keep ENABLE_SD, ENABLE_RTC, ENABLE_AUDIO at 0 for the first ESP32-only test.
5. Build and upload.
6. Open Serial Monitor at 115200 baud.
7. The board will attempt to join the configured Wi-Fi network. If it fails, type
   `AP` in Serial Monitor to start the device hotspot, or `BLE` to start BLE mode.
8. In station mode, find the board IP in Serial Monitor/router DHCP list. In AP mode,
   connect to `Temple-Audio-Setup` and browse to `192.168.4.1`.
9. Dashboard credentials are configured in `config.h`.

## 3. Enable modules later

After you have the wiring and exact GPIOs:

- Set `ENABLE_SD 1`, configure SD_CS_PIN and SPI pins.
- Set `ENABLE_RTC 1`, configure I2C SDA/SCL.
- Set `ENABLE_AUDIO 1`, configure I2S BCLK/LRC/DOUT.
- Keep `ENABLE_BLE 1` if BLE control is wanted.

The SD card must be FAT32 and contain `/audio` and `/schedules` folders (firmware
creates them on first successful mount).

The ILI9488 display and its onboard SD slot may share SPI. Use the display board's
actual pinout and its separate SD chip-select; do not assume generic ESP32 pin numbers.

## 4. API

All HTTP endpoints require Basic Authentication.

- `GET /api/status`
- `POST /api/mode?value=sta` (router Wi-Fi)
- `POST /api/mode?value=ap` (ESP32 access point)
- `POST /api/mode?value=ble` (Wi-Fi off, BLE on)
- `POST /api/stop`
- `POST /api/volume?value=0..21`
- `POST /api/play?file=example.mp3`
- `POST /api/upload` multipart form field `file` (requires SD enabled)

BLE custom service:
- Service UUID: `8f3a0001-6b7c-4a1d-9e2f-123456789001`
- Command characteristic: `8f3a0002-6b7c-4a1d-9e2f-123456789001`
- Status characteristic: `8f3a0003-6b7c-4a1d-9e2f-123456789001`

BLE write commands: `STATUS`, `STOP`, `VOLUME:12`, `MODE:STA`, `MODE:AP`.

## 5. Schedule file format

Create `/schedules/schedule.json` on the SD card:

```json
[
  {
    "id": "morning-suprabhatam",
    "file": "/audio/suprabhatam.mp3",
    "time": "05:00",
    "days": [0,1,2,3,4,5,6],
    "date": "",
    "enabled": true,
    "once": false
  },
  {
    "id": "festival-announcement",
    "file": "/audio/festival.mp3",
    "time": "08:30",
    "days": [],
    "date": "2026-12-25",
    "enabled": true,
    "once": true
  }
]
```

Day numbering: Sunday=0 through Saturday=6. Set timezone in `TZ_INFO` (default India).
For reliable offline scheduling after power loss, enable and wire DS3231, then set its
time correctly. NTP-only time cannot be guaranteed while offline.

## 6. Current limitations to address before production

- Schedule upload/edit/delete endpoints and a full schedule editor UI are not yet included.
- A robust production build should validate uploaded MP3 contents, use safer credential
  provisioning, implement OTA rollback, and add schedule migration/backup.
- The current once-per-day execution marker uses Preferences. Verify event IDs and
  date handling for your final operating rules.
- Wi-Fi reconnect and network-mode switching should be tested with the exact Arduino
  core and NimBLE versions selected by PlatformIO.
- BLE security pairing/bonding is not enabled in this baseline; do not expose control
  BLE to untrusted users until pairing/authentication is added.
- A browser page served over HTTP with Basic Authentication is suitable for bench
  testing on a trusted LAN only. Use stronger network isolation for deployment.
