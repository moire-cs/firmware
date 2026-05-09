#ifndef NRF52_PCNT
#define NRF52_PCNT

#ifdef ARCH_NRF52

#ifdef __cplusplus
extern "C" {
#endif

#include <nrfx.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>

// pcntInit: Initialize pulse counter on../../nrf52_pcnt.h../../nrf52_pcnt.h
// param pin
nrfx_err_t pcntInit(int pin, uint16_t measureTimeMs);

// pcntClearCounter: Resets value of counter to zero
void pcntClearCounter(void);

// pcntClearTimer: Resets value of timer to zero
void pcntClearTimer(void);

// pcntCaptureAndGetCount: Capture current value of counter and return it
// Since we're using PPI to capture the counter value, should not need to used
// this function, and pcntGetCount should be used instead
uint32_t pcntCaptureAndGetCount(void);

// pcntGetCount: Get the last captured value of counter
uint32_t pcntGetCount(void);
#ifdef __cplusplus
}
#endif

#endif // ARCH_NRF52
#endif // NRF52_PCNT
