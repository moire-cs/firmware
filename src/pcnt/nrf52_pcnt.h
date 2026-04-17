#ifndef NRF52_PCNT
#define NRF52_PCNT

#ifdef __cplusplus
extern "C" {
#endif

#include <nrfx.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>

// pcntInit: Initialize pulse counter on../../nrf52_pcnt.h../../nrf52_pcnt.h
// param pin
nrfx_err_t pcntInit(int pin);

// pcntClear: Resets value of counter to zero
void pcntClear(void);

// pcntGetCount: Capture current value of counter and return it
uint32_t pcntGetCount(void);

#ifdef __cplusplus
}
#endif

#endif
