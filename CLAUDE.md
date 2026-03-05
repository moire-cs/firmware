# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the [Meshtastic](https://meshtastic.org) firmware — an open-source LoRa mesh networking project for long-range, low-power communication. It supports ESP32, nRF52, RP2040/RP2350, STM32WL, and Linux/Portduino platforms. Build system is **PlatformIO**.

## Build Commands

```bash
pio run -e tbeam              # Build specific target (tbeam is the default)
pio run -e tbeam -t upload    # Build and flash to device
pio run -e native             # Build native/Linux version
pio test -e native            # Run unit tests
trunk fmt                     # Format code before committing (required)
bin/regen-protos.sh           # Regenerate protobuf code after .proto changes
```

## Code Architecture

### Module System

All features are implemented as **modules** inheriting from `MeshModule` or `ProtobufModule<T>` (`src/mesh/MeshModule.h`, `src/mesh/ProtobufModule.h`):

- `handleReceivedProtobuf()` — process incoming packets
- `allocReply()` — generate response packets
- `runOnce()` — periodic task; returns next run interval in ms
- Register new modules in `src/modules/Modules.cpp`

### Key Source Directories

- `src/mesh/` — core networking: `NodeDB`, `Router`, `Channels`, radio interfaces, and `generated/` (protobuf output)
- `src/modules/` — feature modules (Position, Telemetry, CannedMessage, etc.)
- `src/modules/Telemetry/Sensor/` — I2C sensor drivers, each inheriting `TelemetrySensor`
- `src/gps/` — GPS handling
- `src/graphics/` — display drivers and UI (includes `niche/InkHUD/` for e-ink)
- `src/platform/` — platform-specific code
- `src/concurrency/` — threading utilities (`OSThread`, `Lock`, `SPILock`)

### Hardware Variants

Each variant lives under `variants/<arch>/<name>/` and contains:
- `variant.h` — pin definitions and capability flags (`HAS_GPS`, `USE_SX1262`, etc.)
- `platformio.ini` — build config, usually `extends` a common base env

Custom board JSON files are in `boards/`.

### Configuration System

- `config.*` — device config (LoRa, position, power, display)
- `moduleConfig.*` — per-module config
- `channels.*` — channel management
- Default values: `src/mesh/Default.h` (`Default::getConfiguredOrDefaultMs()`, etc.)
- User-overridable defines: `USERPREFS_*` prefix

### Protobuf Messages

- Source definitions: `protobufs/meshtastic/*.proto`
- Generated output: `src/mesh/generated/`
- Types are prefixed with `meshtastic_` (e.g., `meshtastic_Telemetry`)

### Conditional Compilation

```cpp
#if !MESHTASTIC_EXCLUDE_GPS         // feature exclusion
#ifdef ARCH_ESP32                    // architecture
#if defined(USE_SX1262)             // radio chip
#ifdef HAS_SCREEN                   // hardware capability
#if USERPREFS_EVENT_MODE            // user preference
```

### MQTT Bridge

`src/mqtt/MQTT.cpp` — singleton handling connection and routing. Topic format: `{root}/{channel_id}/{gateway_id}` (default root: `msh`). Controlled per-channel by `uplink_enabled`/`downlink_enabled`.

## Adding a New I2C Sensor

1. Create `src/modules/Telemetry/Sensor/MySensor.h` and `.cpp` inheriting `TelemetrySensor`
2. Implement `initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)` and `getMetrics(meshtastic_Telemetry *measurement)`
3. Register the sensor type in `EnvironmentTelemetry.cpp` and `src/detect/ScanI2C.h`
4. Add the sensor type to the protobuf enum in `protobufs/` if needed, then run `bin/regen-protos.sh`

## Coding Conventions

- Logging: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`
- Classes: `PascalCase`; functions/members: `camelCase`; constants: `UPPER_SNAKE_CASE`
- Use `IF_ROUTER(routerVal, normalVal)` for role-based power defaults
- Respect minimum broadcast intervals on default/public channels via `Default::getConfiguredOrMinimumValue()`
- Run `trunk fmt` before committing — CI enforces formatting via `trunk_check.yml`

## CI

- `main_matrix.yml` — builds all variants on push; uses `bin/generate_ci_matrix.py` to select targets
- `test_native.yml` — runs `pio test -e native`
- PR builds use `--level pr` subset for speed
- Variant support level set via `custom_meshtastic_support_level` in `platformio.ini`
