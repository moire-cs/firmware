# Testing Moire Modules

End-to-end test procedure for `MoireWakeupModule` and `MoireSensorModule` using two Heltec T114 nodes with sensor daughter boards.

## Step 1: Shorten the wakeup interval

In `src/modules/MoireWakeupModule.h`, set a short interval for testing:

```cpp
#define MOIRE_WAKEUP_INTERVAL_MS (30 * 1000UL)  // 30 seconds
```

Change back to `(30 * 60 * 1000UL)` (30 minutes) before deployment.

## Step 2: Flash both devices

```bash
# Gateway T114
pio run -e moire_gateway -t upload

# Sensor node T114
pio run -e moire_sensor_node -t upload
```

## Step 3: Monitor both serial ports simultaneously

```bash
# Gateway
pio device monitor -e moire_gateway

# Sensor node
pio device monitor -e moire_sensor_node
```

## Step 4: Expected output

**Sensor node** (in order):

```
MoireSensor: HDC1080 initialised at 0x40
MoireSensor: OPT3001 initialised at 0x44
MoireSensor: moisture pulse counter initialised on pin 33
MoireWakeup: wakeup received from 0x<gateway_id> seq=0 — triggering sensor read
MoireSensor: starting sensor read
MoireSensor: temp=XX.XX°C  hum=XX.XX%
MoireSensor: lux=XX.XX
MoireSensor: pulseCount=XXXX
MoireSensor: sending — node=0x<id> ...
```

**Gateway:**

```
MoireWakeup: broadcasting wakeup seq=0
MoireSensor: received from 0x<sensor_id> — temp=XX.XX°C ...
MOIRE,<NODEID>,XX.XX,XX.XX,XX.XX,XXXX
```

The CSV line on the gateway serial port is the final output readable by a connected computer:

```
MOIRE,<node_id_hex>,<temp_C>,<humidity_pct>,<lux>,<pulse_count>
```

## Failure diagnosis

| Symptom | Likely cause |
|---|---|
| No `HDC1080 initialised` or `OPT3001 initialised` | I2C wiring issue or address mismatch — check daughter board connection |
| `moisture pulse counter init failed` | Wrong pin or pcnt already claimed — check `MOIRE_MOISTURE_PIN` in `variant.h` |
| Sensor node never logs wakeup received | Devices on different LoRa channels — verify both use the same channel config |
| Gateway never logs `received from` | Packet not reaching gateway — move devices closer together |
| `lux=0.00` while other values are valid | OPT3001 config write failed silently after init succeeded |
