#pragma once

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "detect/ScanI2C.h"
#include "detect/ScanI2CConsumer.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// Private portnum for sensor data packets.
// 256–511 is the reserved private application range — no proto registration needed.
#define MOIRE_SENSOR_PORTNUM ((meshtastic_PortNum)257)

/**
 * Payload broadcast by a sensor node after wakeup.
 * Packed to guarantee a fixed 20-byte on-air size regardless of platform alignment.
 *
 * Field mapping:
 *   temperature  — HDC1080 (°C)
 *   humidity     — HDC1080 (%RH)
 *   lux          — OPT3001 (lux)
 *   pulseCount   — Moire moisture sensor (raw pulse count over 500 ms)
 *   nodeId       — Meshtastic node number of the originating sensor node
 */
struct __attribute__((packed)) MoireSensorPayload {
    uint32_t nodeId;
    float temperature;
    float humidity;
    float lux;
    float pulseCount;
};

/**
 * MoireSensorModule
 *
 * Sensor node role:
 *   Activated by MoireWakeupModule::triggerReading().
 *   Reads HDC1080 (temp/humidity) and OPT3001 (lux) synchronously, then starts the
 *   moisture pulse counter. 500 ms later (via runOnce) the pulse count is captured and
 *   all four values are packed into a MoireSensorPayload and broadcast to the mesh.
 *
 * Gateway role:
 *   Receives MoireSensorPayload packets from sensor nodes.
 *   Logs the values and writes a CSV line to USB serial for the connected computer:
 *     MOIRE,<NODE_ID_HEX>,<temp_C>,<humidity_pct>,<lux>,<pulse_count>
 *
 * All nodes (sensor and gateway):
 *   Returns ProcessMessage::CONTINUE so FloodingRouter relays packets through the mesh.
 */
class MoireSensorModule : public MeshModule, private concurrency::OSThread, public ScanI2CConsumer
{
  public:
    MoireSensorModule();

    /**
     * Start an asynchronous sensor reading cycle.
     * Safe to call from inside handleReceived().
     * Silently ignored if a reading is already in progress.
     */
    void triggerReading();

    // Called by the firmware once the I2C bus scan is complete.
    virtual void i2cScanFinished(ScanI2C *i2cScanner) override;

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    // Two-state machine:
    //   IDLE     — waiting for a triggerReading() call
    //   COUNTING — moisture pulse counter is running; runOnce fires in 500 ms to capture it
    enum class ReadState { IDLE, COUNTING };
    ReadState state = ReadState::IDLE;

    // Fast-sensor results cached between the two runOnce() calls
    float cachedTemp = 0.0f;
    float cachedHumidity = 0.0f;
    float cachedLux = 0.0f;

    bool sensorsReady = false;

    void sendSensorData(float temp, float humidity, float lux, float pulseCount);
};

extern MoireSensorModule *moireSensorModule;

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)
