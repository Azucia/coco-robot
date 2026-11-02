#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace coco_uart {
constexpr uint32_t MAGIC = 0x314F4343;
constexpr uint32_t ACK_MAGIC = 0x414F4343;
constexpr uint32_t LEASE_MS = 250;
enum Kind : uint8_t { Idle = 0, Drive = 1, Stop = 2, Rejected = 3 };
#pragma pack(push, 1)
struct Packet {
    uint32_t magic, session, sequence;
    int16_t throttle, steering;
    uint8_t kind, reserved[3];
    uint32_t crc;
};
#pragma pack(pop)
static_assert(sizeof(Packet) == 24);
inline uint32_t crc32(const void* data, size_t size) {
    uint32_t crc = 0xffffffff;
    const auto* bytes = static_cast<const uint8_t*>(data);
    while (size--) {
        crc ^= *bytes++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return ~crc;
}
inline void seal(Packet& p) { p.crc = crc32(&p, offsetof(Packet, crc)); }
inline bool valid(const Packet& p, uint32_t magic = MAGIC) {
    return p.magic == magic && p.session && p.kind <= Rejected && !p.reserved[0] &&
           !p.reserved[1] && !p.reserved[2] && p.throttle >= -1000 && p.throttle <= 1000 &&
           p.steering >= -1000 && p.steering <= 1000 && p.crc == crc32(&p, offsetof(Packet, crc));
}
class Parser {
    uint8_t bytes_[sizeof(Packet)]{};
    size_t length_ = 0;

public:
    bool feed(uint8_t byte, Packet& result, uint32_t magic = MAGIC) {
        if (length_ == sizeof(bytes_)) {
            memmove(bytes_, bytes_ + 1, sizeof(bytes_) - 1);
            --length_;
        }
        bytes_[length_++] = byte;
        if (length_ != sizeof(bytes_))
            return false;
        memcpy(&result, bytes_, sizeof(result));
        if (!valid(result, magic))
            return false;
        length_ = 0;
        return true;
    }
};
class Receiver {
    uint32_t session_ = 0, sequence_ = 0, received_ = 0;
    bool have_sequence_ = false;

public:
    bool active = false;
    int16_t throttle = 0, steering = 0;
    void stop() {
        active = false;
        throttle = steering = 0;
    }
    bool accept(const Packet& p, uint32_t now) {
        if (!valid(p) || p.kind == Rejected)
            return false;
        if (p.session != session_) {
            if (p.kind == Drive)
                return false;  // Boot must begin with Idle/Stop.
            session_ = p.session;
            have_sequence_ = false;
            stop();
        }
        if (have_sequence_ && int32_t(p.sequence - sequence_) <= 0)
            return false;
        sequence_ = p.sequence;
        have_sequence_ = true;
        if (p.kind == Stop)
            stop();
        if (p.kind == Drive) {
            throttle = p.throttle;
            steering = p.steering;
            received_ = now;
            active = true;
        }
        return true;
    }
    bool expire(uint32_t now) {
        if (active && uint32_t(now - received_) >= LEASE_MS) {
            stop();
            return true;
        }
        return false;
    }
};
inline bool selfTest() {
    if (crc32("123456789", 9) != 0xcbf43926)
        return false;
    Packet p{};
    p.magic = MAGIC;
    p.session = 1;
    p.kind = Drive;
    p.throttle = 1000;
    seal(p);
    Receiver receiver;
    if (receiver.accept(p, 0))
        return false;
    p.kind = Idle;
    seal(p);
    if (!receiver.accept(p, 0))
        return false;
    p.kind = Drive;
    p.sequence = 1;
    seal(p);
    if (!receiver.accept(p, 0xfffffff0) || !receiver.active)
        return false;
    if (receiver.accept(p, 0xfffffff1))
        return false;
    if (receiver.expire(0x30) || !receiver.active)
        return false;
    if (!receiver.expire(0xea) || receiver.active)
        return false;
    p.sequence = 2;
    p.throttle = 1001;
    seal(p);
    if (valid(p) || receiver.accept(p, 1))
        return false;
    p.throttle = 1000;
    p.kind = Stop;
    seal(p);
    if (!receiver.accept(p, 2) || receiver.active)
        return false;
    p.crc ^= 1;
    if (valid(p))
        return false;
    seal(p);
    Parser parser;
    Packet decoded{};
    for (int i = 0; i < 31; ++i)
        if (parser.feed(0xa5, decoded))
            return false;
    const auto* bytes = reinterpret_cast<const uint8_t*>(&p);
    bool found = false;
    for (size_t i = 0; i < sizeof(p); ++i)
        found = parser.feed(bytes[i], decoded) || found;
    return found && decoded.kind == Stop && decoded.sequence == 2;
}
}  // namespace coco_uart
