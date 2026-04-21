#include "configuration.h"

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR) || defined(MOIRE_ROUTER)

#include "../PowerStatus.h"
#include "MeshService.h"
#include "MoireSensorModule.h"
#include "NodeDB.h"
#include "Router.h"
#include "sleep.h"
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
#if !defined(MOIRE_GATEWAY) && !defined(MOIRE_ROUTER)

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

    config.device.role = meshtastic_Config_DeviceConfig_Role_SENSOR;
    config.power.is_power_saving = true;

#endif // !MOIRE_GATEWAY && !MOIRE_ROUTER

    // We set our default setttings for the Moire Devices after I2C is completed
    // to make sure we override any defaults
    if (config.lora.region == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
        LOG_WARN("LoRa Region unset, Moire defaulting to US, please ensure you are "
                 "complying with local regulations");
        config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_US;
    }

    // Turn off led heartbeat
    config.device.led_heartbeat_disabled = true;

    // Set our maximum hops to 5, we can increase in the field if needed, but
    // Let's keep i <= 5 in NOLA
    config.lora.hop_limit = 5;
}

// ---------------------------------------------------------------------------
// Sensor reading — called from MoireWakeupModule::handleReceived()
// ---------------------------------------------------------------------------

void MoireSensorModule::triggerReading(uint32_t sleepTimeMs)
{
    if (state != ReadState::IDLE) {
        LOG_DEBUG("MoireSensor: read already in progress, ignoring trigger");
        return;
    }
    if (!sensorsReady) {
        LOG_WARN("MoireSensor: sensors not ready, skipping read");
        return;
    }

    cachedSleepTimeMs = sleepTimeMs;
    wakeupReceivedAt = millis();
    LOG_INFO("Received Sleep Time: %d ms", cachedSleepTimeMs);

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
//            fires 2000 ms after capturing pulse count and sending to mesh to
//            go to sleep
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

        // We set the state to sending, send the mesh packet to the queue, then
        // schedule runOnce() to be called again in two seconds, where the sleep
        // route is then taken
        state = ReadState::SENDING;
        sendSensorData(cachedTemp, cachedHumidity, cachedLux, (float)pulseCount, cachedBatteryPercentage);

        // Stay awake 30 seconds after sending so the radio has time to transmit
        // and relay any other nodes' packets before sleeping.
        return 30000;
    }

    else if (state == ReadState::SENDING) {
        state = ReadState::IDLE;
        uint32_t elapsed = millis() - wakeupReceivedAt;
        uint32_t actualSleep = (elapsed < cachedSleepTimeMs) ? (cachedSleepTimeMs - elapsed) : 1000;
        LOG_INFO("Done sending: elapsed=%u ms, sleeping for %u ms", elapsed, actualSleep);
        doDeepSleep(actualSleep, true, false);
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
    payload.nodeVersion = MOIRE_VERSION;
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
    // If you add a new version, simply use this same format with new sensor IDs
    switch (payload.nodeVersion) {
    case (MOIRE_BOARD_2026):
        Serial.printf("Node version 1 detected\r\n");
        Serial.printf("MESSAGE:{\"nodeId\": \"0x%08x\",\"sensors\": "
                      "[\"0\",\"1\",\"2\",\"3\",\"4\"],"
                      "\"messages\": [[%.0f,%.2f,%.2f,%.2f,%d]]}\r\n",
                      payload.nodeId, payload.pulseCount, payload.temperature, payload.humidity, payload.lux,
                      payload.batteryPercentage);
        break;
    }
#endif

#ifdef MOIRE_ROUTER
    LOG_INFO("Moire Router: Rebroadcasting reading from 0x%08x", payload.nodeId);
#endif

    return ProcessMessage::CONTINUE;
}

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR) ||
       // defined(MOIRE_ROUTER)
