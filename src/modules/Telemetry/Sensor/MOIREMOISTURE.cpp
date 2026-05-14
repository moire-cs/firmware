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
    status = (pcntInit(MOIRE_MOISTURE_PIN, MEASURE_TIME_MS) == NRFX_SUCCESS);

    initI2CSensor();
    return status;
}

bool MOIREMOISTURESensor::getMetrics(meshtastic_Telemetry *measurement)
{
    uint32_t pulseCount;

    pcntClearTimer();
    pcntClearCounter();
    delay(MEASURE_TIME_MS + 100);
    pulseCount = pcntGetCount();

    // We use soil temperature to transmit pulseCount since soil moisture is an 8
    // bit unsigned

    measurement->variant.environment_metrics.has_soil_temperature = true;
    measurement->variant.environment_metrics.soil_temperature = pulseCount;

    return true;
}

#endif
