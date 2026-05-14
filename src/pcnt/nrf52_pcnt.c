#ifdef NRF52_SERIES
#include <nrfx.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>

// GLOBAL VARS
static nrfx_timer_t counter = NRFX_TIMER_INSTANCE(2);
static nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);
static nrf_ppi_channel_t counterChannelPPI;
static nrf_ppi_channel_t captureChannelPPI;
static nrf_timer_cc_channel_t countChannel = NRF_TIMER_CC_CHANNEL0;
static nrf_timer_cc_channel_t compareChannel = NRF_TIMER_CC_CHANNEL1;

// COUNTER - This hardware timer will be used as a counter to keep track of
// pulses
static nrfx_err_t setupCounter(void)
{
    // Set up counter
    nrfx_timer_config_t counterConfig;
    counterConfig.bit_width = NRF_TIMER_BIT_WIDTH_32;
    counterConfig.frequency = NRF_TIMER_FREQ_16MHz;
    counterConfig.mode = NRF_TIMER_MODE_COUNTER;
    counterConfig.interrupt_priority = 2;
    counterConfig.p_context = NULL;

    nrfx_err_t err = nrfx_timer_init(&counter, &counterConfig, NULL);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    nrfx_timer_enable(&counter);
    return NRFX_SUCCESS;
}

// TIMER - This hardware timer will be used to make sure the counter measures
// for whatever measureTime is
static nrfx_err_t setupTimer(uint16_t measureTimeMs)
{
    // Set up timer
    nrfx_timer_config_t timerConfig;
    timerConfig.bit_width = NRF_TIMER_BIT_WIDTH_32;
    timerConfig.mode = NRF_TIMER_MODE_TIMER;
    timerConfig.frequency = NRF_TIMER_FREQ_1MHz;
    timerConfig.interrupt_priority = 2;
    timerConfig.p_context = NULL;

    nrfx_err_t err = nrfx_timer_init(&timer, &timerConfig, NULL);
    if (err != NRFX_SUCCESS)
        return err;

    // Set the timer compare event to trigger on measure time.
    // No CPU interrupt needed — PPI captures the counter autonomously.
    uint32_t measureTimeTicks = nrfx_timer_ms_to_ticks(&timer, measureTimeMs);
    nrfx_timer_compare(&timer, compareChannel, measureTimeTicks, false);

    nrfx_timer_enable(&timer);
    return err;
}

// GPIOTE
static nrfx_err_t setupGPIOTE(nrfx_gpiote_pin_t pulsePin)
{
    nrfx_err_t err = NRFX_SUCCESS;
    if (!nrfx_gpiote_is_init()) {
        err = nrfx_gpiote_init(1);
        if (err != NRFX_SUCCESS) {
            return err;
        }
    }

    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(1);
    config.pull = NRF_GPIO_PIN_PULLUP;
    config.hi_accuracy = true;

    err = nrfx_gpiote_in_init(pulsePin, &config, NULL);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    nrfx_gpiote_in_event_enable(pulsePin, false);

    return NRFX_SUCCESS;
}

// PPI: GPIOTE in event -> Counter Count Task
static nrfx_err_t setupCounterPPI(nrfx_gpiote_pin_t pulsePin)
{
    nrfx_err_t err = nrfx_ppi_channel_alloc(&counterChannelPPI);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = nrfx_ppi_channel_assign(counterChannelPPI, nrfx_gpiote_in_event_addr_get(pulsePin),
                                  nrfx_timer_task_address_get(&counter, NRF_TIMER_TASK_COUNT));
    if (err != NRFX_SUCCESS) {
        return err;
    }

    return nrfx_ppi_channel_enable(counterChannelPPI);
}

// PPI: Timer Compare Event -> Counter Capture Task
// This will capture the counter value when measureTimeMs has elapsed
static nrfx_err_t setupCapturePPI()
{
    nrfx_err_t err = nrfx_ppi_channel_alloc(&captureChannelPPI);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = nrfx_ppi_channel_assign(captureChannelPPI, nrfx_timer_compare_event_address_get(&timer, compareChannel),
                                  nrfx_timer_task_address_get(&counter, NRF_TIMER_TASK_CAPTURE0));

    if (err != NRFX_SUCCESS)
        return err;

    return nrfx_ppi_channel_enable(captureChannelPPI);
}

// Public Functions
nrfx_err_t pcntInit(int pin, uint16_t measureTimeMs)
{
    nrfx_gpiote_pin_t pulsePin = (nrfx_gpiote_pin_t)pin;

    nrfx_err_t err = setupCounter();
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = setupTimer(measureTimeMs);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = setupGPIOTE(pulsePin);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = setupCapturePPI();
    if (err != NRFX_SUCCESS) {
        return err;
    }

    return setupCounterPPI(pulsePin);
}

uint32_t pcntCaptureAndGetCount()
{
    nrfx_timer_capture(&counter, countChannel);
    return nrfx_timer_capture_get(&counter, countChannel);
}

uint32_t pcntGetCount()
{
    return nrfx_timer_capture_get(&counter, countChannel);
}

void pcntClearCounter()
{
    nrfx_timer_clear(&counter);
}
void pcntClearTimer()
{
    nrfx_timer_clear(&timer);
}
#endif // NRF52_SERIES
