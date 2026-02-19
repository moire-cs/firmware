#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && MOIRE_MOISTURE_SENSOR

#include "../../../pcnt/nrf52_pcnt.h"
#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "MOIREMOISTURE.h"
#include "TelemetrySensor.h"

MOIREMOISTURESensor::MOIREMOISTURESensor() : TelemetrySensor(meshtastic_TelemetrySensorType_SENSOR_UNSET, "MOIREMOISTURE") {}

bool MOIREMOISTURESensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    status = (pcntInit(MOIRE_MOISTURE_PIN) == NRFX_SUCCESS);

    initI2CSensor();
    return status;
}

bool MOIREMOISTURESensor::getMetrics(meshtastic_Telemetry *measurement)
{
    measurement->variant.environment_metrics.has_soil_moisture = true;
    uint32_t pulseCount;
    pcntClear();
    pulseCount = pcntGetCount();
    // Convert pulses to moisture level
    uint8_t moistureLevel = 25;

    measurement->variant.environment_metrics.soil_moisture = moistureLevel;

    return true;
}

#endif
