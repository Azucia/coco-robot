#pragma once
#include "DriveState.h"

namespace coco {
// Independent lease: head events never keep the treads moving, or vice versa.
struct HeadState {
    Wheel wheel;
    bool armed = false, active = false;
    int16_t target = 0;
    uint32_t receivedAt = 0, leaseMs = 0, lastStep = 0;
    void stop(uint32_t now) {
        wheel.stop(now);
        target = 0;
        active = armed = false;
    }
    bool command(int16_t value, uint32_t at, uint32_t lease, uint32_t now) {
        if ((value != 0 && value != 1000 && value != -1000) || !lease || lease > 250 ||
            uint32_t(now - at) >= lease)
            return false;
        if (!value) {
            wheel.stop(now);
            target = 0;
            active = false;
            armed = true;
            return true;
        }
        if (!armed)
            return false;
        target = value > 0 ? MAX_DUTY : -MAX_DUTY;
        receivedAt = at;
        leaseMs = lease;
        active = true;
        return true;
    }
    void tick(uint32_t now, bool validLink) {
        if (!validLink || (active && uint32_t(now - receivedAt) >= leaseMs)) {
            stop(now);
            return;
        }
        if (uint32_t(now - lastStep) < RAMP_MS)
            return;
        lastStep = now;
        wheel.update(target, now);
    }
};
}  // namespace coco
