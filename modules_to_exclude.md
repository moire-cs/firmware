# Modules to Exclude — Moire Sensor Node (Heltec T114)

For maximum battery life on the moisture-sensing node, which wakes, reads, transmits, and sleeps.

## Build Flags for `moire_sensor_node/platformio.ini`

```ini
build_flags = ${nrf52840_base.build_flags}
  -Ivariants/nrf52840/moire_sensor_node
  -DHELTEC_T114
  -DMOIRE_MOISTURE_SENSOR
  # Battery savings exclusions:
  -DMESHTASTIC_EXCLUDE_GPS=1
  -DMESHTASTIC_EXCLUDE_ATAK=1
  -DMESHTASTIC_EXCLUDE_CANNEDMESSAGES=1
  -DMESHTASTIC_EXCLUDE_REPLYBOT=1
  -DMESHTASTIC_EXCLUDE_REMOTEHARDWARE=1
  -DMESHTASTIC_EXCLUDE_POWERSTRESS=1
  -DMESHTASTIC_EXCLUDE_TRACEROUTE=1
  -DMESHTASTIC_EXCLUDE_WAYPOINT=1
  -DMESHTASTIC_EXCLUDE_EXTERNALNOTIFICATION=1
  -DMESHTASTIC_EXCLUDE_RANGETEST=1
  -DMESHTASTIC_EXCLUDE_SERIAL=1
  -DMESHTASTIC_EXCLUDE_NEIGHBORINFO=1
  -DMESHTASTIC_EXCLUDE_DETECTIONSENSOR=1
  -DMESHTASTIC_EXCLUDE_DROPZONE=1
  -DMESHTASTIC_EXCLUDE_STATUS=1
  -DMESHTASTIC_EXCLUDE_PKI=1
```

## Exclusions Ranked by Impact

| Module                         | Reason                                                                                 | Impact   |
| ------------------------------ | -------------------------------------------------------------------------------------- | -------- |
| `EXCLUDE_GPS`                  | T114 has an L76K GPS polled periodically — biggest single win for a static sensor node | **High** |
| `EXCLUDE_NEIGHBORINFO`         | Periodically broadcasts neighbor packets — radio TX is expensive                       | **High** |
| `EXCLUDE_EXTERNALNOTIFICATION` | Has a polling `runOnce` thread                                                         | Medium   |
| `EXCLUDE_TRACEROUTE`           | Responds to trace packets, minor CPU wake overhead                                     | Medium   |
| `EXCLUDE_DETECTIONSENSOR`      | Has its own polling thread; `MoireSensorModule` replaces this role                     | Medium   |
| `EXCLUDE_ATAK`                 | Not needed on a sensor node                                                            | Low      |
| `EXCLUDE_CANNEDMESSAGES`       | UI feature, not needed                                                                 | Low      |
| `EXCLUDE_REPLYBOT`             | Not needed                                                                             | Low      |
| `EXCLUDE_WAYPOINT`             | Not needed                                                                             | Low      |
| `EXCLUDE_REMOTEHARDWARE`       | Not needed                                                                             | Low      |
| `EXCLUDE_RANGETEST`            | Testing only                                                                           | Low      |
| `EXCLUDE_SERIAL`               | Not needed if not using the serial data module                                         | Low      |
| `EXCLUDE_PKI`                  | Saves RAM/flash; only exclude if not using encrypted channels                          | Low      |
| `EXCLUDE_POWERSTRESS`          | Testing-only module                                                                    | Trivial  |
| `EXCLUDE_DROPZONE`             | Not needed                                                                             | Trivial  |
| `EXCLUDE_STATUS`               | Not needed                                                                             | Trivial  |

## Modules to Keep

| Module                                   | Reason                                 |
| ---------------------------------------- | -------------------------------------- |
| `ADMIN`                                  | Required for over-mesh reconfiguration |
| `NODEINFO`                               | Required for mesh participation        |
| `TEXTMESSAGE`                            | Core mesh packet handling              |
| `ENVIRONMENTAL_SENSOR` / `HAS_TELEMETRY` | Sensor data pipeline                   |
| `GENERIC_THREAD_MODULE`                  | May be required by `MoireWakeupModule` |

## Note on GPS Hardware

`EXCLUDE_GPS` removes the firmware module only. The L76K GPS on the T114 is powered via `VEXT_ENABLE` (pin 21). The existing T114 variant power management already handles keeping this pin low during sleep, so hardware power-down should be covered automatically.
