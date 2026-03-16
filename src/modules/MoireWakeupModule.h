#pragma once

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// How often the gateway broadcasts a wakeup signal.
// Change this value to experiment with different duty cycles.
// NOTE: set to 30s for testing, change to (30 * 60 * 1000UL) for deployment
#define MOIRE_WAKEUP_INTERVAL_MS (30 * 1000UL)

// Private portnum for wakeup packets.
// 256–511 is the reserved private application range — no proto registration
// needed.
#define MOIRE_WAKEUP_PORTNUM ((meshtastic_PortNum)256)

/** Payload broadcast by gateway node
 *
 * Field mapping:
 * sleepTimeMs -- The amount of time in milliseconds that the sensor node should
 * sleep for before recieving the next wakeup signal
 */
struct __attribute__((packed)) MoireWakeupPayload {
    uint32_t sleepTimeMs;
};

/**
 * MoireWakeupModule
 *
 * Gateway role:
 *   Broadcasts a wakeup packet to the mesh every MOIRE_WAKEUP_INTERVAL_MS.
 *   Packet contains a sleep interval which will tell the sensor device how
 *   long to sleep before expecting the next packet
 *
 * Sensor node role:
 *   Receives the wakeup packet, triggers MoireSensorModule::triggerReading(),
 *   then returns CONTINUE so FloodingRouter rebroadcasts to the rest of the
 * mesh. Once wakeup message rebroadcasted and sensor readings sent, go to sleep
 * for time specified in wakeup packet
 */
class MoireWakeupModule : public MeshModule, private concurrency::OSThread
{
  public:
    MoireWakeupModule();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    void sendWakeup();
};

extern MoireWakeupModule *moireWakeupModule;

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)
