#pragma once
#include <stdint.h>
namespace coco {
// Physical GPIO numbers, not connector positions. No voice/audio peripherals in this build.
// SIX-IN wiring: all PWM/STBY pins are externally held high, never driven by this sketch.
// Defaults are an example only; production firmware requires a saved PINS command before driving.
constexpr uint8_t DEFAULT_PINS[] = {4,5,6,7,8,9}; // AIN1,AIN2,BIN1,BIN2,CIN1,CIN2
inline bool validPins(const uint8_t *pins) {
  for(unsigned i=0;i<6;++i) {
    uint8_t p = pins[i];
    if (p==13 || p==14) return false; // UART1 owns these two pins.
    if (!((p>=4 && p<=18) || (p>=38 && p<=42) || p==47)) return false;
    for(unsigned j=0;j<i;++j) if(p==pins[j]) return false;
  }
  return true;
}
constexpr bool INVERT_A = false, INVERT_B = true;
constexpr uint32_t PWM_HZ = 10000;
constexpr uint8_t PWM_BITS = 8;
constexpr int16_t MAX_DUTY = 255; // Full duty requested for the 4.4V motor supply.
constexpr int16_t START_DUTY = 255;
constexpr uint32_t START_BOOST_MS = 100;
constexpr uint32_t RAMP_MS = 10, REVERSE_PAUSE_MS = 120;
constexpr int16_t RAMP_STEP = 4;
static_assert(MAX_DUTY > 0 && MAX_DUTY <= 255, "Duty limit outside 8-bit range");
}
