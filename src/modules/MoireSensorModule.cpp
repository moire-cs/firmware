#include "configuration.h"

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)

#include "../PowerStatus.h"
#include "MeshService.h"
#include "MoireSensorModule.h"
#include "NodeDB.h"
#include "Router.h"
#include <Arduino.h>
#include <string.h>

// I2C sensor libraries — guarded so they compile only when the library is
// present. Both HDC1080 and OPT3001 come from the same ClosedCube package, so
// checking for either header is sufficient.
#if __has_include(<ClosedCube_HDC1080.h>)
#include <ClosedCube_HDC1080.h>
#define MOIRE_HAS_HDC1080
static ClosedCube_HDC1080 hdc1080;
#endif

#if __has_include(<ClosedCube_OPT3001.h>)
#include <ClosedCube_OPT3001.h>
#define MOIRE_HAS_OPT3001
static ClosedCube_OPT3001 opt3001;
#endif

#ifdef MOIRE_MOISTURE_SENSOR
#include "pcnt/nrf52_pcnt.h"
#endif

MoireSensorModule::MoireSensorModule() : MeshModule("MoireSensor"), concurrency::OSThread("MoireSensor"), ScanI2CConsumer()
{
    // The thread is disabled on construction.
    // It is only re-enabled by triggerReading() → setIntervalFromNow(500).
}

// ---------------------------------------------------------------------------
// I2C scan callback — initialise sensors on sensor nodes only
// ---------------------------------------------------------------------------

void MoireSensorModule::i2cScanFinished(ScanI2C *i2cScanner)
{
#ifndef MOIRE_GATEWAY

#ifdef MOIRE_HAS_HDC1080
    ScanI2C::FoundDevice hdc1080Dev = i2cScanner->find(ScanI2C::DeviceType::HDC1080);
    if (hdc1080Dev.type != ScanI2C::DeviceType::NONE) {
        hdc1080.begin(hdc1080Dev.address.address);
        LOG_INFO("MoireSensor: HDC1080 initialised at 0x%02x", hdc1080Dev.address.address);
        sensorsReady = true;
    } else {
        LOG_WARN("MoireSensor: HDC1080 not found on I2C bus");
    }
#endif

#ifdef MOIRE_HAS_OPT3001
    ScanI2C::FoundDevice opt3001Dev = i2cScanner->find(ScanI2C::DeviceType::OPT3001);
    if (opt3001Dev.type != ScanI2C::DeviceType::NONE) {
        opt3001.begin(opt3001Dev.address.address);
        // Continuous conversion, auto-range, 100 ms conversion time, latch mode
        OPT3001_Config cfg;
        cfg.RangeNumber = 0b1100;
        cfg.ConvertionTime = 0b0;
        cfg.Latch = 0b1;
        cfg.ModeOfConversionOperation = 0b11;
        opt3001.writeConfig(cfg);
        LOG_INFO("MoireSensor: OPT3001 initialised at 0x%02x", opt3001Dev.address.address);
        sensorsReady = true;
    } else {
        LOG_WARN("MoireSensor: OPT3001 not found on I2C bus");
    }
#endif

#ifdef MOIRE_MOISTURE_SENSOR
    if (pcntInit(MOIRE_MOISTURE_PIN) == NRFX_SUCCESS) {
        LOG_INFO("MoireSensor: moisture pulse counter initialised on pin %d", MOIRE_MOISTURE_PIN);
        sensorsReady = true;
    } else {
        LOG_WARN("MoireSensor: moisture pulse counter init failed on pin %d", MOIRE_MOISTURE_PIN);
    }
#endif

#endif // !MOIRE_GATEWAY
    if (config.lora.region == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
        LOG_WARN("LoRa Region unset, Moire defaulting to US, please ensure you are "
                 "complying with local regulations");
        config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_US;
    }
}

// ---------------------------------------------------------------------------
// Sensor reading — called from MoireWakeupModule::handleReceived()
// ---------------------------------------------------------------------------

void MoireSensorModule::triggerReading()
{
    if (state != ReadState::IDLE) {
        LOG_DEBUG("MoireSensor: read already in progress, ignoring trigger");
        return;
    }
    if (!sensorsReady) {
        LOG_WARN("MoireSensor: sensors not ready, skipping read");
        return;
    }

    LOG_INFO("MoireSensor: starting sensor read");
    cachedBatteryPercentage = powerStatus->getBatteryChargePercent();
    LOG_DEBUG("MoireSensor: BatteryPercentage=%d%%", cachedBatteryPercentage);

    // Read temperature and humidity (fast I2C reads, done synchronously)
#ifdef MOIRE_HAS_HDC1080
    cachedTemp = hdc1080.readTemperature();
    cachedHumidity = hdc1080.readHumidity();
    LOG_DEBUG("MoireSensor: temp=%.2f°C  hum=%.2f%%", cachedTemp, cachedHumidity);
#else
    cachedTemp = 0.0f;
    cachedHumidity = 0.0f;
#endif

    // Read ambient light (fast I2C read, done synchronously)
#ifdef MOIRE_HAS_OPT3001
    OPT3001 result = opt3001.readResult();
    cachedLux = result.lux;
    LOG_DEBUG("MoireSensor: lux=%.2f", cachedLux);
#else
    cachedLux = 0.0f;
#endif

    // Start the moisture pulse counter.
    // The counter must run for exactly 500 ms before being read — we do this
    // non-blocking by scheduling runOnce() to fire 500 ms from now.
#ifdef MOIRE_MOISTURE_SENSOR
    pcntClear();
    state = ReadState::COUNTING;
    setIntervalFromNow(500);
#else
    // No moisture sensor on this build — send immediately with pulseCount = 0
    sendSensorData(cachedTemp, cachedHumidity, cachedLux, 0.0f, cachedBatteryPercentage);
#endif
}

// ---------------------------------------------------------------------------
// OSThread — fires 500 ms after triggerReading() to capture the pulse count
// ---------------------------------------------------------------------------

int32_t MoireSensorModule::runOnce()
{
    if (state == ReadState::COUNTING) {
#ifdef MOIRE_MOISTURE_SENSOR
        uint32_t pulseCount = pcntGetCount();
#else
        uint32_t pulseCount = 0;
#endif
        LOG_DEBUG("MoireSensor: pulseCount=%u", pulseCount);
        state = ReadState::IDLE;
        sendSensorData(cachedTemp, cachedHumidity, cachedLux, (float)pulseCount, cachedBatteryPercentage);
    }

    // Park the thread until the next triggerReading() call.
    // Using INT32_MAX (not disable()) so setIntervalFromNow(500) can reschedule
    // it.
    return INT32_MAX;
}

// ---------------------------------------------------------------------------
// Packet construction and transmission
// ---------------------------------------------------------------------------

void MoireSensorModule::sendSensorData(float temp, float humidity, float lux, float pulseCount, uint8_t batteryPercentage)
{
    MoireSensorPayload payload = {};
    payload.nodeId = nodeDB->getNodeNum();
    payload.temperature = temp;
    payload.humidity = humidity;
    payload.lux = lux;
    payload.pulseCount = pulseCount;
    payload.batteryPercentage = batteryPercentage;

    meshtastic_MeshPacket *p = router->allocForSending();
    p->decoded.portnum = MOIRE_SENSOR_PORTNUM;
    memcpy(p->decoded.payload.bytes, &payload, sizeof(payload));
    p->decoded.payload.size = sizeof(payload);
    p->to = NODENUM_BROADCAST;
    p->decoded.want_response = false;
    p->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    LOG_INFO("MoireSensor: sending — node=0x%08x  temp=%.2f°C  hum=%.2f%%  "
             "lux=%.2f  pulse=%.0f  bat=%d%%",
             payload.nodeId, temp, humidity, lux, pulseCount, batteryPercentage);

    service->sendToMesh(p, RX_SRC_LOCAL, true);
}

// ---------------------------------------------------------------------------
// Incoming packet handling
// ---------------------------------------------------------------------------

bool MoireSensorModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p->decoded.portnum == MOIRE_SENSOR_PORTNUM;
}

ProcessMessage MoireSensorModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.payload.size < sizeof(MoireSensorPayload)) {
        LOG_WARN("MoireSensor: malformed packet from 0x%08x (size=%u, expected=%u)", mp.from, mp.decoded.payload.size,
                 sizeof(MoireSensorPayload));
        return ProcessMessage::CONTINUE;
    }

    MoireSensorPayload payload;
    memcpy(&payload, mp.decoded.payload.bytes, sizeof(payload));

    LOG_INFO("MoireSensor: received from 0x%08x — temp=%.2f°C  hum=%.2f%%  "
             "lux=%.2f  pulse=%.0f  bat=%d%%",
             payload.nodeId, payload.temperature, payload.humidity, payload.lux, payload.pulseCount, payload.batteryPercentage);

#ifdef MOIRE_GATEWAY
    // Write a CSV line to USB serial so a connected computer can read the data.
    // Format:
    // MOIRE,<node_id_hex>,<temp_C>,<humidity_pct>,<lux>,<pulse_count>,<battery_pct>
    Serial.printf("MOIRE,%08X,%.2f,%.2f,%.2f,%.0f,%d\r\n", payload.nodeId, payload.temperature, payload.humidity, payload.lux,
                  payload.pulseCount, payload.batteryPercentage);
#endif

    // CONTINUE lets FloodingRouter relay this packet toward the gateway (or
    // beyond).
    return ProcessMessage::CONTINUE;
}

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)
