#pragma once
#include "coco_uart_protocol.h"

namespace coco_dialogue {
constexpr uint32_t MAGIC = 0x444f4343;
constexpr uint8_t PLAY = 1, NEXT = 2, STOP = 3;
constexpr int MAX_SLOT = 16;

// Reverse UART messages never enter the motor's Drive receiver. They reuse its
// packet envelope and CRC, bound to the live voice-board session and sequence.
inline bool valid(const coco_uart::Packet& p, uint32_t session) {
    return coco_uart::valid(p, MAGIC) && p.session == session && p.steering == 0 &&
           ((p.kind == PLAY && p.throttle >= 1 && p.throttle <= MAX_SLOT) ||
            ((p.kind == NEXT || p.kind == STOP) && p.throttle == 0));
}
class Receiver {
    uint32_t sequence_ = 0;
    bool seen_ = false;
public:
    bool accept(const coco_uart::Packet& p, uint32_t session) {
        if (!coco_dialogue::valid(p, session) || (seen_ && int32_t(p.sequence - sequence_) <= 0)) return false;
        sequence_ = p.sequence;
        seen_ = true;
        return true;
    }
};
}  // namespace coco_dialogue
