
#pragma once
#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && MOIRE_MOISTURE_SENSOR

#include "../../../pcnt/nrf52_pcnt.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"

#define MEASURE_TIME_MS 500

class MOIREMOISTURESensor : public TelemetrySensor
{
  public:
    MOIREMOISTURESensor();

    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
    virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
};

#endif
