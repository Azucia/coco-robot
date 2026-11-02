#include "CocoState.h"
#include "coco_dialogue_protocol.h"
#include <cassert>
#include <cstdio>

int main() {
    assert(coco_uart::selfTest());
    assert(walle::selfTest());
    coco_dialogue::Receiver events;
    coco_uart::Packet e{};
    e.magic=coco_dialogue::MAGIC;e.session=4;e.sequence=10;
    e.kind=coco_dialogue::PLAY;e.throttle=1;coco_uart::seal(e);
    assert(!events.accept(e,5));
    assert(events.accept(e,4));
    assert(!events.accept(e,4));
    ++e.sequence;e.throttle=17;coco_uart::seal(e);assert(!events.accept(e,4));
    e.throttle=1;e.crc^=1;assert(!events.accept(e,4));
    coco_uart::seal(e);assert(events.accept(e,4));
    coco_uart::Receiver motors;
    assert(!motors.accept(e,0)); // Presentation events cannot drive a motor.
    e.sequence++;e.throttle=0;e.kind=coco_dialogue::STOP;coco_uart::seal(e);
    assert(events.accept(e,4));
    e.sequence++;e.kind=coco_dialogue::NEXT;coco_uart::seal(e);assert(events.accept(e,4));

    uint8_t mac[]={2,3,4,5,6,7};
    coco::State remote;
    walle::Packet p{};p.kind=walle::Kind::PairRequest;p.session=3;p.nonce=8;
    assert(remote.pair(mac,p,0));
    p.kind=walle::Kind::Control;p.sequence=1;p.leaseMs=250;p.throttle=0;
    assert(remote.control(mac,p,1,1));
    p.sequence=2;p.throttle=1000;assert(remote.control(mac,p,2,2));
    // A dialogue action is not a drive command and does not renew its lease.
    p.kind=walle::Kind::Dialogue;p.sequence=3;p.throttle=1;
    walle::seal(p);walle::Packet decoded{};assert(walle::decode(&p,sizeof(p),decoded));
    assert(!remote.control(mac,p,3,3));
    remote.tick(252);assert(!remote.hasControl && remote.throttle==0);
    puts("PASS: motion lease, CRC, session, replay, slot bounds, and dialogue isolation");
}
