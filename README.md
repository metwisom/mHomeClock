# mHome — Wi-Fi Desktop Clock

A desktop clock built on ESP8266 and a MAX7219 LED matrix. Syncs time over NTP, connects to your Wi-Fi through a captive portal on first boot, and exposes a password-protected web UI for settings, live status, brightness control, and OTA firmware updates.

## Features

- **NTP time sync** with configurable timezone offset, auto-resync every hour.
- **Captive-portal Wi-Fi setup** on first boot (or after a reset) — no hardcoded credentials, no reflashing to change networks.
- **Password-protected web UI** (session-cookie based) reachable at `http://mhome.local` or the device's IP.
  - **Status tab**: live clock, uptime, NTP sync age, and a collapsible system info panel (SSID, firmware version/build/tag, IP, mDNS, MAC, RSSI, WiFi state, free RAM, flash usage, boot count, reset reason).
  - **Settings tab**: auto/manual brightness (LDR-based auto mode with EMA smoothing, or a manual slider), live RU/EN language switching, reboot, Wi-Fi reset, and firmware upload.
  - Live-updating values (clock, uptime, RSSI, brightness, …) via polling — no page reloads needed for day-to-day use.
- **OTA firmware update** — upload a `.bin` straight from the web UI, no USB required after the first flash.
- **mDNS** — reachable as `mhome.local` on the local network.
- **RU/EN localization**, switchable live in the UI, persisted to the device.
- **Auto-incrementing build number + build tag** baked into the firmware at compile time, shown in the UI (see [Build tooling](#build-tooling)).

## Hardware

| Part | Notes |
|---|---|
| ESP8266 board | NodeMCU v2 (or compatible NodeMCU 1.0 / ESP-12E board) |
| LED matrix | 4x MAX7219 FC-16 modules chained (32x8 total), driven via `MD_Parola` / `MD_MAX72XX` |
| Light sensor | LDR + resistor divider into `A0`, used for automatic brightness |

### Wiring

| Signal | Pin |
|---|---|
| Matrix CS | `D6` |
| Matrix CLK | `D5` (hardware SPI) |
| Matrix DIN | `D7` (hardware SPI) |
| LDR | `A0` |

## Getting started

### Build

Requires [PlatformIO](https://platformio.org/). The build scripts install it for you if it's missing.

```powershell
# Windows
.\scripts\build.ps1            # build only
.\scripts\build.ps1 --upload   # build and flash over USB
.\scripts\build.ps1 --monitor  # also open the serial monitor
```

```bash
# macOS/Linux/Git Bash
./scripts/build.sh              # build only
./scripts/build.sh --upload     # build and flash over USB
./scripts/build.sh --monitor    # also open the serial monitor
```

Both scripts check for PlatformIO, install it via `pip` if missing, then run `pio pkg install` (platform + libraries), `pio run`, and optionally upload/monitor.

### First boot

1. Flash the firmware and power up the device.
2. It starts an open access point named `mHome_XXX`. Connect to it from your phone or laptop.
3. A captive portal should open automatically (or browse to `http://192.168.4.1`). Pick your Wi-Fi network, enter its password, set a **settings password** (used to log into the device's web UI later), and pick a default interface language.
4. The device reboots and joins your network. Find it at `http://mhome.local` (or check your router's client list / the serial monitor for its IP).

### Using the web UI

- Log in with the settings password you chose during setup.
- **Status** tab: live clock and diagnostics.
- **Settings** tab: brightness, language, reboot, Wi-Fi reset, firmware upload.
- **Reset WiFi Settings** wipes the stored network and settings password and puts the device back into captive-portal setup mode.

### Updating firmware over the air

Build a new `.bin` (`pio run` produces `.pio/build/nodemcuv2/firmware.bin`), then upload it from the Settings tab. The page shows upload progress and automatically redirects once the device comes back online.

## Build tooling

- `platformio.ini` — build flags include `FW_VERSION`, set here.
- `scripts/extra_script.py` — a PlatformIO pre-build hook that:
  - auto-increments a build counter on every build, and resolves a `BUILD_TAG` from the `BUILD_TAG` environment variable (one-off override, never saved) or the `build_tag` field in `build.json`. Falls back to `"custom"` if neither is set. `build.json` is tracked in git, so the counter and tag travel with the repo — override `BUILD_TAG` as an env var for a one-off build without touching the committed file.

Both show up in the web UI's system info panel as `<version> #<build> by <tag>`.

## Project structure

```
src/main.cpp                Firmware source (WiFi, web server, display, EEPROM, OTA, i18n)
src/bigFont.h                Custom LED-matrix font (digits, colon, dash)
platformio.ini                Build configuration
build.json                Build counter + build tag (tracked in git)
scripts/extra_script.py                Build-time version/tag stamping
scripts/build.ps1, scripts/build.sh    Cross-platform build/upload helpers
```

## License

MIT — see [LICENSE](LICENSE).
