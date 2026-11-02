#pragma once
#include "MotorConfig.h"
namespace coco {
inline int sign(int value) { return (value > 0) - (value < 0); }
inline int16_t toward(int16_t value, int16_t target) {
  if (value < target) return target-value > RAMP_STEP ? value+RAMP_STEP : target;
  if (value > target) return value-target > RAMP_STEP ? value-RAMP_STEP : target;
  return value;
}
struct Wheel {
  int16_t output = 0;
  int8_t lastSign = 0;
  uint32_t stoppedAt = 0;
  uint32_t startedAt = 0;
  bool boosting = false;
  void stop(uint32_t now) {
    if (output) { lastSign = sign(output); stoppedAt = now; }
    output = 0;
    boosting = false;
  }
  void update(int16_t target, uint32_t now) {
    if (!target) { stop(now); return; }
    if (output && sign(output) != sign(target)) { stop(now); return; }
    if (!output && lastSign && lastSign != sign(target) && uint32_t(now-stoppedAt) < REVERSE_PAUSE_MS) return;
    if (!output) { startedAt = now; boosting = true; }
    if (boosting) {
      if (uint32_t(now-startedAt) < START_BOOST_MS) {
        output = sign(target) * START_DUTY;
      } else {
        boosting = false;
        output = target;
      }
    } else {
      output = toward(output, target);
    }
    if (output) lastSign = sign(output);
  }
};
struct DriveState {
  Wheel left, right;
  bool neutralSeen = false;
  int16_t targetLeft = 0, targetRight = 0;
  uint32_t lastStep = 0;
  void stop(uint32_t now, bool rearm = true) {
    left.stop(now); right.stop(now); targetLeft = targetRight = 0;
    if (rearm) neutralSeen = false;
  }
  void command(int16_t throttle, int16_t steering, uint32_t now) {
    if (throttle < -1000 || throttle > 1000 || steering < -1000 || steering > 1000) { stop(now); return; }
    if (!throttle && !steering) { stop(now, false); neutralSeen = true; return; }
    if (!neutralSeen) { stop(now, false); return; }
    // Steering < 0 means left: left tread reverses and right tread advances.
    int32_t l = int32_t(throttle) + steering, r = int32_t(throttle) - steering;
    int32_t peakL = l < 0 ? -l : l, peakR = r < 0 ? -r : r;
    int32_t scale = peakL > peakR ? peakL : peakR;
    if (scale < 1000) scale = 1000;
    targetLeft = int16_t(l * MAX_DUTY / scale); targetRight = int16_t(r * MAX_DUTY / scale);
    if (!targetLeft) left.stop(now);
    if (!targetRight) right.stop(now);
  }
  void tick(uint32_t now, bool validControl) {
    if (!validControl) { stop(now); return; }
    if (uint32_t(now-lastStep) < RAMP_MS) return;
    lastStep = now;
    left.update(targetLeft, now); right.update(targetRight, now);
  }
};
}
