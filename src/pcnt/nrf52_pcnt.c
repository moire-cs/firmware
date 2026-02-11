#include <nrfx.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>

// GLOBAL VARS
static nrfx_timer_t counter = NRFX_TIMER_INSTANCE(2);
static nrf_ppi_channel_t ppiChannel;
static nrf_timer_cc_channel_t timerChannel = NRF_TIMER_CC_CHANNEL0;

// TIMER
static nrfx_err_t setupCounter(void)
{
    // Set up counter
    nrfx_timer_config_t counterConfig;
    counterConfig.bit_width = NRF_TIMER_BIT_WIDTH_16;
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

// PPI: GPIOTE in event -> Timer Count Task
static nrfx_err_t setupPPI(nrfx_gpiote_pin_t pulsePin)
{
    nrfx_err_t err = nrfx_ppi_channel_alloc(&ppiChannel);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = nrfx_ppi_channel_assign(ppiChannel, nrfx_gpiote_in_event_addr_get(pulsePin),
                                  nrfx_timer_task_address_get(&counter, NRF_TIMER_TASK_COUNT));
    if (err != NRFX_SUCCESS) {
        return err;
    }

    return nrfx_ppi_channel_enable(ppiChannel);
}

// Public Functions
nrfx_err_t pcntInit(int pin)
{
    nrfx_gpiote_pin_t pulsePin = (nrfx_gpiote_pin_t)pin;

    nrfx_err_t err = setupCounter();
    if (err != NRFX_SUCCESS) {
        return err;
    }

    err = setupGPIOTE(pulsePin);
    if (err != NRFX_SUCCESS) {
        return err;
    }

    return setupPPI(pulsePin);
}

uint32_t pcntGetCount()
{
    nrfx_timer_capture(&counter, timerChannel);
    return nrfx_timer_capture_get(&counter, timerChannel);
}

void pcntClear()
{
    nrfx_timer_clear(&counter);
}
