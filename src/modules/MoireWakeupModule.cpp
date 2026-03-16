#include "configuration.h"

#if defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)

#include "MeshService.h"
#include "MoireSensorModule.h"
#include "MoireWakeupModule.h"
#include "NodeDB.h"
#include "Router.h"

MoireWakeupModule::MoireWakeupModule() : MeshModule("MoireWakeup"), concurrency::OSThread("MoireWakeup")
{
#ifdef MOIRE_GATEWAY
    // Send the first wakeup 10 seconds after boot so the mesh has time to settle,
    // then every MOIRE_WAKEUP_INTERVAL_MS afterwards.
    setIntervalFromNow(10 * 1000);
#endif
    // On sensor nodes runOnce() is never scheduled — the thread only wakes when
    // MoireSensorModule's state machine calls setIntervalFromNow() on its own
    // thread. Nothing to do here.
}

bool MoireWakeupModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p->decoded.portnum == MOIRE_WAKEUP_PORTNUM;
}

// ----- Gateway: periodic broadcast ----------------------------------------

void MoireWakeupModule::sendWakeup()
{
    LOG_INFO("MoireWakeup: broadcasting wakeup");

    MoireWakeupPayload payload = {};
    payload.sleepTimeMs = 30 * 1000 * 60;
    meshtastic_MeshPacket *p = router->allocForSending();
    p->decoded.portnum = MOIRE_WAKEUP_PORTNUM;

    memcpy(p->decoded.payload.bytes, &payload, sizeof(payload));
    p->decoded.payload.size = sizeof(payload);

    p->to = NODENUM_BROADCAST;
    p->decoded.want_response = false;
    p->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(p, RX_SRC_LOCAL, true);
}

int32_t MoireWakeupModule::runOnce()
{
#ifdef MOIRE_GATEWAY
    sendWakeup();
    return MOIRE_WAKEUP_INTERVAL_MS;
#else
    // Sensor nodes never use this thread — disable permanently.
    return disable();
#endif
}

// ----- Sensor node: receive and trigger ------------------------------------

ProcessMessage MoireWakeupModule::handleReceived(const meshtastic_MeshPacket &mp)
{
#ifndef MOIRE_GATEWAY
    if (mp.decoded.payload.size < sizeof(MoireWakeupPayload)) {
        LOG_WARN("MoireWakeup: malformed packet from 0x%08x (size=%u)", mp.from, mp.decoded.payload.size);
        return ProcessMessage::CONTINUE;
    }

    MoireWakeupPayload payload;
    memcpy(&payload, mp.decoded.payload.bytes, sizeof(payload));

    LOG_INFO("MoireWakeup: received from 0x%08x sleepTime=%d — triggering sensor read", mp.from, payload.sleepTimeMs);

    if (moireSensorModule)
        moireSensorModule->triggerReading();
#endif

    // CONTINUE lets FloodingRouter rebroadcast this packet so other nodes also
    // wake up.
    return ProcessMessage::CONTINUE;
}

#endif // defined(MOIRE_GATEWAY) || defined(MOIRE_MOISTURE_SENSOR)
