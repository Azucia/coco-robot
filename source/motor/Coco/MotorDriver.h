#pragma once
#include <Arduino.h>
#include "DriveState.h"
namespace coco {
class MotorDriver {
  uint8_t pins[6]{};
  bool attached[6]{}, usable = false, initialized = false;
  int16_t previousA = 32767, previousB = 32767;
  void wheel(uint8_t in1, uint8_t in2, int16_t value, bool invert) {
    // IN-pin PWM alternates drive and high-impedance coast with external PWM/STBY=HIGH.
    ledcWrite(in1, 0); ledcWrite(in2, 0);
    int16_t physical = invert ? -value : value;
    if (physical > 0) ledcWrite(in1, physical);
    if (physical < 0) ledcWrite(in2, -physical);
  }
 public:
  void halt() {
    if (!initialized) return;
    for(unsigned i=0;i<6;++i) {
      if(attached[i]) ledcWrite(pins[i],0);
      else digitalWrite(pins[i],LOW);
    }
    previousA = previousB = 0;
  }
  bool begin(const uint8_t *map = DEFAULT_PINS) {
    if(!validPins(map)) return false;
    for(unsigned i=0;i<6;++i) pins[i]=map[i];
    initialized = true; usable = true;
    for(uint8_t pin : pins) {digitalWrite(pin,LOW);pinMode(pin,OUTPUT);}
    for(unsigned i=0;i<6;++i) {
      attached[i]=ledcAttach(pins[i],PWM_HZ,PWM_BITS);
      if(!attached[i]) usable=false;
    }
    halt(); return usable;
  }
  void apply(const DriveState &drive) {
    if (!usable) {halt();return;}
    ledcWrite(pins[4],0);ledcWrite(pins[5],0); // Torso always stopped in v0.2.
    if (drive.left.output == previousA && drive.right.output == previousB) return;
    wheel(pins[0],pins[1],drive.left.output,INVERT_A);
    wheel(pins[2],pins[3],drive.right.output,INVERT_B);
    previousA = drive.left.output; previousB = drive.right.output;
  }
};
}
