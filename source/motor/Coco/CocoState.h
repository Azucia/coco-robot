#pragma once
#include "WallEProtocol.h"

// Hardware-independent receiver state. No motor GPIOs are enabled in v0.1.
namespace coco {
constexpr uint8_t CHANNEL = 6;
constexpr uint32_t LINK_TIMEOUT_MS = 1500;
struct State {
    uint8_t remote[6]{};
    bool bound = false, established = false, hasControl = false;
    uint32_t session = 0, nonce = 0, sequence = 0, lastRx = 0, lease = 0;
    int16_t throttle = 0, steering = 0;
    bool same(const uint8_t* mac) const { return memcmp(remote, mac, 6) == 0; }
    bool allowed(const uint8_t* mac) const { return !bound || same(mac); }
    void stop() {
        throttle = steering = 0;
        hasControl = false;
    }
    bool pair(const uint8_t* mac, const walle::Packet& p, uint32_t now) {
        if (p.kind != walle::Kind::PairRequest || !p.session || !p.nonce || !allowed(mac))
            return false;
        // Retransmitted PairRequest must not erase accepted control sequence/state.
        if (established && same(mac) && session == p.session && nonce == p.nonce)
            return true;
        memcpy(remote, mac, 6);
        bound = established = true;
        session = p.session;
        nonce = p.nonce;
        sequence = 0;
        lastRx = now;
        lease = 0;
        stop();
        return true;
    }
    bool control(const uint8_t* mac, const walle::Packet& p, uint32_t receivedAt, uint32_t now) {
        if (!established || !same(mac) || p.kind != walle::Kind::Control || p.session != session ||
            p.nonce != nonce || !p.leaseMs || p.leaseMs > walle::CONTROL_LEASE_MS ||
            !walle::newerSequence(p.sequence, sequence) || uint32_t(now - receivedAt) >= p.leaseMs)
            return false;
        sequence = p.sequence;
        lastRx = receivedAt;
        lease = p.leaseMs;
        throttle = p.throttle;
        steering = p.steering;
        hasControl = true;
        return true;
    }
    bool auxiliary(const uint8_t* mac, const walle::Packet& p, uint32_t receivedAt, uint32_t now) {
        bool head = p.kind == walle::Kind::Head && p.steering == 0 &&
                    (p.throttle == 0 || p.throttle == 1000 || p.throttle == -1000);
        bool event = p.kind == walle::Kind::Dialogue &&
                     ((p.steering == 0 && p.throttle >= 1 && p.throttle <= 16) ||
                      ((p.steering == 1 || p.steering == 2 || p.steering == 3) && p.throttle == 0));
        if ((!head && !event) || !established || !same(mac) || p.session != session ||
            p.nonce != nonce || !p.leaseMs || p.leaseMs > walle::CONTROL_LEASE_MS ||
            uint32_t(now - receivedAt) >= p.leaseMs || !walle::newerSequence(p.sequence, sequence))
            return false;
        sequence = p.sequence;  // Neither drive lease nor link heartbeat is renewed.
        return true;
    }
    void tick(uint32_t now) {
        if (hasControl && uint32_t(now - lastRx) >= lease)
            stop();
        if (established && uint32_t(now - lastRx) >= LINK_TIMEOUT_MS) {
            stop();
            established = false;
        }
    }
    walle::Packet reply(walle::Kind kind, uint32_t replySession, uint32_t replyNonce) const {
        walle::Packet p{};
        p.kind = kind;
        p.sequence = sequence;
        p.session = replySession;
        p.nonce = replyNonce;
        memcpy(p.name, "Coco", 5);
        walle::seal(p);
        return p;
    }
};
}  // namespace coco
