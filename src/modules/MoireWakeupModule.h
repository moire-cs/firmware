#pragma once

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// How often the gateway broadcasts a wakeup signal.
// Change this value to experiment with different duty cycles.
#define MOIRE_WAKEUP_INTERVAL_MS (30 * 60 * 1000UL)

// Private portnum for wakeup packets.
// 256–511 is the reserved private application range — no proto registration needed.
#define MOIRE_WAKEUP_PORTNUM ((meshtastic_PortNum)256)

/**
 * MoireWakeupModule
 *
 * Gateway role:
 *   Broadcasts a wakeup packet to the mesh every MOIRE_WAKEUP_INTERVAL_MS.
 *   The 4-byte payload is a monotonically increasing sequence number.
 *
 * Sensor node role:
 *   Receives the wakeup packet, triggers MoireSensorModule::triggerReading(),
 *   then returns CONTINUE so FloodingRouter rebroadcasts to the rest of the mesh.
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
    uint32_t seqNum = 0;
    void sendWakeup();
};

extern MoireWakeupModule *moireWakeupModule;

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)
