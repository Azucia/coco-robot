#include <cassert>
#include <cstdio>
#include "CocoState.h"
#include "HeadState.h"
#include "coco_dialogue_protocol.h"

int main() {
    coco::HeadState head;
    assert(!head.command(1000, 0, 250, 0));  // No movement on boot without neutral.
    assert(head.command(0, 0, 250, 0));
    assert(head.command(1000, 10, 250, 10));
    head.tick(20, true);
    assert(head.wheel.output == 255);
    head.tick(259, true);
    assert(head.wheel.output != 0);
    head.tick(260, true);
    assert(head.wheel.output == 0 && !head.armed);
    assert(!head.command(1000, 261, 250, 261));  // Stale hold cannot restart.
    assert(head.command(0, 262, 250, 262));
    assert(head.command(1000, 263, 250, 263));
    head.tick(273, true);
    assert(head.wheel.output == 255);
    assert(head.command(0, 274, 250, 274));
    assert(head.wheel.output == 0);
    assert(head.command(-1000, 275, 250, 275));
    head.tick(285, true);
    assert(head.wheel.output == 0);  // Direction reversal retains 120ms dwell.
    head.tick(395, true);
    assert(head.wheel.output == -255);
    head.tick(400, false);
    assert(head.wheel.output == 0 && !head.armed);
    assert(!head.command(1, 401, 250, 401));
    assert(!head.command(0, 0, 250, 250));

    uint8_t mac[] = {2, 3, 4, 5, 6, 7}, wrong[] = {2, 3, 4, 5, 6, 8};
    coco::State state;
    walle::Packet p{};
    p.kind = walle::Kind::PairRequest;
    p.session = 3;
    p.nonce = 8;
    assert(state.pair(mac, p, 0));
    p.kind = walle::Kind::Control;
    p.sequence = 1;
    p.leaseMs = 250;
    p.throttle = 1000;
    assert(state.control(mac, p, 10, 10));
    p.kind = walle::Kind::Head;
    p.sequence = 2;
    assert(!state.auxiliary(wrong, p, 20, 20));
    p.nonce++;
    assert(!state.auxiliary(mac, p, 20, 20));
    p.nonce--;
    p.session++;
    assert(!state.auxiliary(mac, p, 20, 20));
    p.session--;
    assert(!state.auxiliary(mac, p, 20, 270));
    assert(state.auxiliary(mac, p, 20, 20));
    assert(!state.auxiliary(mac, p, 20, 20));
    p.sequence = 3;
    p.kind = walle::Kind::Dialogue;
    p.throttle = 0;
    p.steering = 3;  // Remote wake action.
    assert(state.auxiliary(mac, p, 30, 30));
    state.tick(260);
    assert(!state.hasControl && !state.throttle);  // Head/wake cannot extend tread lease.
    walle::seal(p);
    walle::Packet decoded{};
    assert(walle::decode(&p, sizeof(p), decoded));
    p.crc ^= 1;
    assert(!walle::decode(&p, sizeof(p), decoded));

    coco_uart::Packet wake{};
    wake.magic = coco_dialogue::MAGIC;
    wake.session = 5;
    wake.sequence = 2;
    wake.kind = coco_dialogue::WAKE;
    coco_uart::seal(wake);
    coco_dialogue::Receiver receiver;
    assert(receiver.accept(wake, 5));
    assert(!receiver.accept(wake, 5));
    coco_uart::Receiver motors;
    assert(!motors.accept(wake, 0));
    wake.throttle = 1;
    coco_uart::seal(wake);
    assert(!coco_dialogue::valid(wake, 5));
    walle::Packet offer{};
    offer.kind = walle::Kind::Offer;
    offer.steering = 6;
    assert(walle::offerChannel(offer, 5) == 6);  // Sender channel wins over RX metadata.
    offer.steering = 0;
    assert(walle::offerChannel(offer, 6) == 6);  // Compatible with old robots.
    offer.steering = 12;
    assert(walle::offerChannel(offer, 6) == 0);
    puts("PASS: head release/lease/rearm/reversal; wake isolation; advertised pairing channel");
}
