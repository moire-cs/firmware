#pragma once

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR) || defined(MOIRE_ROUTER)

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// How often the gateway broadcasts a wakeup signal.
#define MOIRE_WAKEUP_INTERVAL_MINS 30
#define MOIRE_WAKEUP_INTERVAL_MS (MOIRE_WAKEUP_INTERVAL_MINS * 60 * 1000UL)

// TODO: We might need to make this even greater depending on how long devices
// need to be on to rebroadcast readings from further out nodes
#if MOIRE_WAKEUP_INTERVAL_MS < 60000
#error                                                                                                                           \
    "Do not set wakeup signal interval below 1 minute, we will always subtract 30 seconds from the sleep time to give the sensor nodes time to wake up"
#endif

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

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR) ||
       // defined(MOIRE_ROUTER)
