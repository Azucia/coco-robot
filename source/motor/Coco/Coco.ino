#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <atomic>
#include "CocoState.h"
#include "MotorDriver.h"
#include "coco_uart_protocol.h"
#include "coco_dialogue_protocol.h"

namespace {
coco::State state;
coco::DriveState drive;
coco::MotorDriver motors;
Preferences prefs;
struct Rx { uint8_t mac[6]; walle::Packet packet; uint32_t at; };
QueueHandle_t queue = nullptr;
std::atomic_bool busy{false};
std::atomic_uint dropped{0};
uint32_t txAt = 0, heartbeatAt = 0, logAt = 0;
bool ready = false, storageOK = false;
bool mappingConfigured = false, configPending = false;
uint8_t activePins[6]{};
char command[64]{};
size_t commandLength = 0;
coco_uart::Parser uartParser;
coco_uart::Receiver uartReceiver;
bool uartSuppressed = false;
uint32_t voiceSession=0, voiceSequence=0, voiceLastAt=0, dialogueSequence=0;
bool dialogueSequenceSeen=false;
void dialogue(int slot) {
  if (!voiceSession || uint32_t(millis()-voiceLastAt)>500) {
    Serial0.println("DIALOGUE_REJECT voice_link_unavailable"); return;
  }
  if (dialogueSequenceSeen && voiceSequence==dialogueSequence) return;
  coco_uart::Packet p{};
  p.magic=coco_dialogue::MAGIC;p.session=voiceSession;p.sequence=voiceSequence;
  p.kind=slot>0?coco_dialogue::PLAY:(slot==0?coco_dialogue::NEXT:coco_dialogue::STOP);
  p.throttle=slot>0?slot:0;
  coco_uart::seal(p);
  if (!coco_dialogue::valid(p,voiceSession)) return;
  dialogueSequence=voiceSequence;dialogueSequenceSeen=true;
  Serial1.write(reinterpret_cast<const uint8_t*>(&p),sizeof(p));
  Serial0.printf("DIALOGUE_SENT slot=%d\n",slot);
}

void stopAll() {
  uartReceiver.stop();
  state.stop(); drive.stop(millis()); motors.halt();
}

void uartTick() {
  if (uartReceiver.expire(millis())) {
    drive.stop(millis()); motors.halt();
    Serial0.println("UART_TIMEOUT stop=1");
  }
  for (unsigned count=0; count<256 && Serial1.available(); ++count) {
    coco_uart::Packet p{};
    if (!uartParser.feed(Serial1.read(),p)) continue;
    bool reject = p.kind == coco_uart::Drive &&
      (!ready || !mappingConfigured || configPending || uartSuppressed ||
       (state.hasControl && (state.throttle || state.steering)));
    if (reject) uartSuppressed=true; // Rejected actions cannot resume after remote release.
    bool wasActive = uartReceiver.active;
    bool accepted = !reject && uartReceiver.accept(p,millis());
    if (accepted) {
      if(voiceSession!=p.session) dialogueSequenceSeen=false;
      voiceSession=p.session;voiceSequence=p.sequence;voiceLastAt=millis();
      if (p.kind != coco_uart::Drive) uartSuppressed=false;
      if (p.kind == coco_uart::Stop || (wasActive && !uartReceiver.active)) {
        stopAll();
        if (wasActive) Serial0.println("UART_STOP all_IN=0");
      }
      if (p.kind == coco_uart::Drive) {
        if (!wasActive) {
          drive.command(0,0,millis());
          Serial0.printf("UART_CONTROL active=1 throttle=%d steering=%d\n",p.throttle,p.steering);
        }
        drive.command(p.throttle,p.steering,millis());
      }
    }
    p.magic=coco_uart::ACK_MAGIC;
    if (!accepted) p.kind=coco_uart::Rejected;
    coco_uart::seal(p);
    Serial1.write(reinterpret_cast<const uint8_t*>(&p),sizeof(p));
  }
}

void received(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!queue || !info || !info->src_addr || !data || len != sizeof(walle::Packet)) return;
  Rx event{}; memcpy(event.mac, info->src_addr, 6); memcpy(&event.packet, data, sizeof(event.packet));
  event.at = millis();
  if (xQueueSend(queue, &event, 0) != pdTRUE) ++dropped;
}
void sent(const esp_now_send_info_t *, esp_now_send_status_t) { busy.store(false); }
bool peer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  // Keep at most one transmit peer; discovery from many remotes cannot fill the peer table.
  esp_now_peer_info_t previous{};
  while (esp_now_fetch_peer(true, &previous) == ESP_OK) esp_now_del_peer(previous.peer_addr);
  esp_now_peer_info_t p{}; memcpy(p.peer_addr, mac, 6);
  p.channel = coco::CHANNEL; p.ifidx = WIFI_IF_STA; p.encrypt = false;
  return esp_now_add_peer(&p) == ESP_OK;
}
bool send(const uint8_t *mac, const walle::Packet &p) {
  if (!ready || busy.load() || !peer(mac)) return false;
  busy.store(true); txAt = millis();
  esp_err_t result = esp_now_send(mac, reinterpret_cast<const uint8_t *>(&p), sizeof(p));
  if (result != ESP_OK) { busy.store(false); Serial0.printf("TX_ERROR code=%d\n", result); return false; }
  return true;
}
void status() {
  Serial0.printf("COCO MOTOR v0.2 name=Coco radio=%u bound=%u linked=%u channel=6 throttle=%d steering=%d dropped=%lu uptime=%lu\n",
    ready, state.bound, state.established, state.throttle, state.steering,
    (unsigned long)dropped.load(), (unsigned long)millis());
  Serial0.printf("DRIVE neutral_armed=%u left_pwm=%d right_pwm=%d max_pwm=%d\n",drive.neutralSeen,drive.left.output,drive.right.output,coco::MAX_DUTY);
  Serial0.printf("PINMAP configured=%u pending_reboot=%u active=%u,%u,%u,%u,%u,%u order=AIN1,AIN2,BIN1,BIN2,CIN1,CIN2\n",
    mappingConfigured,configPending,activePins[0],activePins[1],activePins[2],activePins[3],activePins[4],activePins[5]);
  Serial0.printf("UART_CONTROL active=%u throttle=%d steering=%d tx=13 rx=14 baud=115200\n",
    uartReceiver.active,uartReceiver.throttle,uartReceiver.steering);
}
void configurePins(const char *line) {
  unsigned values[6]{}; char extra;
  if(sscanf(line,"PINS %u %u %u %u %u %u %c",&values[0],&values[1],&values[2],&values[3],&values[4],&values[5],&extra)!=6) {
    Serial0.println("PINS_ERROR syntax: PINS AIN1 AIN2 BIN1 BIN2 CIN1 CIN2");return;
  }
  uint8_t proposed[6]{};
  for(unsigned i=0;i<6;++i) {if(values[i]>255) {Serial0.println("PINS_ERROR range");return;} proposed[i]=values[i];}
  if(!coco::validPins(proposed)) {Serial0.println("PINS_ERROR duplicate/reserved; allowed GPIO4-18,38-42,47");return;}
  // Configuration writes always stop motion first and require a reboot to resume.
  stopAll();configPending=true;
  if(!storageOK || prefs.putBytes("pinmap",proposed,6)!=6) {Serial0.println("PINS_ERROR storage; motion locked");return;}
  Serial0.printf("PINS_SAVED %u,%u,%u,%u,%u,%u; send REBOOT to apply\n",proposed[0],proposed[1],proposed[2],proposed[3],proposed[4],proposed[5]);
}
void serialTick() {
  for (int count = 0; count < 64 && Serial0.available(); ++count) {
    char c = Serial0.read();
    if (c == '\r') continue;
    if (c == '\n') {
      command[commandLength] = 0;
      if (!strcmp(command, "STATUS")) status();
      else if (!strcmp(command,"PINS?")) {
        status(); uint8_t saved[6]{};
        if(storageOK && prefs.getBytes("pinmap",saved,6)==6)
          Serial0.printf("PINS_SAVED %u,%u,%u,%u,%u,%u\n",saved[0],saved[1],saved[2],saved[3],saved[4],saved[5]);
      }
      else if (!strncmp(command,"PINS ",5)) configurePins(command);
      else if (!strcmp(command,"REBOOT")) {stopAll();Serial0.flush();ESP.restart();}
      else if (!strcmp(command,"STOP")) stopAll();
      else if (!strcmp(command,"DIALOG NEXT")) dialogue(0);
      else if (!strcmp(command,"DIALOG STOP")) dialogue(-1);
      else if (!strncmp(command,"DIALOG ",7)) {
        int slot=0;char extra;
        if(sscanf(command,"DIALOG %d %c",&slot,&extra)==1 && slot>=1 && slot<=16) dialogue(slot);
      }
      else if (!strcmp(command, "UNPAIR")) {
        stopAll();
        if (!storageOK || (prefs.isKey("remote") && !prefs.remove("remote"))) {
          Serial0.println("UNPAIR_FAILED storage=error");
        } else {
          if (state.bound) esp_now_del_peer(state.remote);
          state = coco::State{}; Serial0.println("UNPAIRED pairing=open");
        }
      }
      commandLength = 0;
    } else if (commandLength < sizeof(command)-1) command[commandLength++] = c;
  }
}
void process(const Rx &event) {
  walle::Packet p{};
  if (!walle::decode(&event.packet, sizeof(event.packet), p) ||
      uint32_t(millis()-event.at) >= walle::CONTROL_LEASE_MS || (event.mac[0] & 1)) return;
  if (p.kind == walle::Kind::Discover && p.nonce && state.allowed(event.mac)) {
    send(event.mac, state.reply(walle::Kind::Offer, 0, p.nonce));
  } else if (p.kind == walle::Kind::PairRequest && state.allowed(event.mac) && p.session && p.nonce) {
    // Persist first, then acknowledge. Never claim a binding that failed to save.
    if (!state.bound && (!storageOK || prefs.putBytes("remote", event.mac, 6) != 6)) {
      Serial0.println("PAIR_REJECT storage=error"); return;
    }
    bool fresh = !state.established || state.session != p.session || state.nonce != p.nonce;
    if (fresh) { stopAll(); uartSuppressed=true; }
    if (state.pair(event.mac, p, millis())) {
      send(event.mac, state.reply(walle::Kind::PairAck, p.session, p.nonce));
      if (fresh) Serial0.printf("PAIR_ACCEPT name=Coco remote=%02X:%02X:%02X:%02X:%02X:%02X session=%lu\n",
        event.mac[0],event.mac[1],event.mac[2],event.mac[3],event.mac[4],event.mac[5],(unsigned long)p.session);
    }
  } else if (p.kind == walle::Kind::Dialogue) {
    if (state.established && state.same(event.mac) &&
        p.session==state.session && p.nonce==state.nonce &&
        p.leaseMs && p.leaseMs<=walle::CONTROL_LEASE_MS &&
        uint32_t(millis()-event.at)<p.leaseMs &&
        walle::newerSequence(p.sequence,state.sequence) &&
        ((p.steering==0 && p.throttle>=1 && p.throttle<=16) ||
         ((p.steering==1 || p.steering==2) && p.throttle==0))) {
      state.sequence=p.sequence;
      dialogue(p.steering==0?p.throttle:(p.steering==1?0:-1));
    }
  } else if (p.kind == walle::Kind::Control) {
    int16_t oldT = state.throttle, oldS = state.steering;
    if (state.control(event.mac, p, event.at, millis())) {
      if(mappingConfigured && !configPending) {
        if (state.throttle || state.steering) {
          if (uartReceiver.active) { uartReceiver.stop(); uartSuppressed=true; }
          drive.command(state.throttle,state.steering,millis());
        } else if (!uartReceiver.active) {
          drive.command(0,0,millis());
        }
        drive.tick(millis(),true); motors.apply(drive);
      }
      if (oldT != state.throttle || oldS != state.steering)
        Serial0.printf("CONTROL throttle=%d steering=%d neutral_armed=%u\n",state.throttle,state.steering,drive.neutralSeen);
    }
  }
}
}

void setup() {
  Serial0.begin(115200); delay(200);
  Serial1.begin(115200,SERIAL_8N1,14,13);
  if (!coco_uart::selfTest()) { Serial0.println("FATAL UART protocol_selftest"); return; }
  Serial0.println("UART protocol self-test PASS");
  Serial0.println("COCO_BOOT MOTOR v0.2 SIX-IN ESP32-S3; commands: STATUS / UNPAIR / PINS? / PINS / REBOOT");
  if (!walle::selfTest()) { Serial0.println("FATAL protocol_selftest"); return; }
  storageOK = prefs.begin("coco-link", false);
  if(storageOK && prefs.getBytesLength("pinmap")==6) {
    mappingConfigured = prefs.getBytes("pinmap",activePins,6)==6 && coco::validPins(activePins);
  }
  if(mappingConfigured && !motors.begin(activePins)) {Serial0.println("FATAL pwm_init; all IN=0");return;}
  if(!mappingConfigured) Serial0.println("MOTOR_LOCKED configure PINS first; no motor GPIOs driven");
  if (storageOK && prefs.getBytesLength("remote") == 6) {
    state.bound = prefs.getBytes("remote", state.remote, 6) == 6;
  }
  queue = xQueueCreate(16, sizeof(Rx));
  if (!queue || !WiFi.mode(WIFI_STA)) { Serial0.println("FATAL wifi_or_queue"); return; }
  WiFi.setAutoReconnect(false); WiFi.disconnect(false, false); WiFi.setSleep(false);
  if (esp_wifi_set_channel(coco::CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK || esp_now_init() != ESP_OK ||
      esp_now_register_recv_cb(received) != ESP_OK || esp_now_register_send_cb(sent) != ESP_OK) {
    Serial0.println("FATAL espnow_init"); return;
  }
  ready = true;
  Serial0.printf("COCO_READY name=Coco mac=%s channel=6 protocol=1 bytes=48 pairing=%s motor_output=TB6612_SIX_IN max_pwm=%d torso=stopped\n",
    WiFi.macAddress().c_str(), state.bound ? "bound-remote-only" : "open", coco::MAX_DUTY);
}
void loop() {
  serialTick();
  uartTick();
  if (!ready) { delay(10); return; }
  uint32_t now = millis();
  if (busy.load() && uint32_t(now-txAt) > 150) busy.store(false);
  // Enforce the lease before processing queued work, using each packet's receive timestamp.
  bool moving = state.throttle || state.steering, linked = state.established;
  state.tick(now);
  if(mappingConfigured && !configPending) {
    drive.tick(now,uartReceiver.active || (state.established && state.hasControl)); motors.apply(drive);
  }
  if (moving && !state.hasControl) Serial0.println("CONTROL_TIMEOUT stop=1 all_IN=0");
  if (linked && !state.established) Serial0.println("LINK_TIMEOUT waiting_for_reconnect=1");
  Rx event{};
  for (int i = 0; i < 16 && xQueueReceive(queue, &event, 0) == pdTRUE; ++i) process(event);
  now = millis();
  if (state.established && uint32_t(now-heartbeatAt) >= walle::ROBOT_HEARTBEAT_MS) {
    if (send(state.remote, state.reply(walle::Kind::Heartbeat, state.session, state.nonce))) heartbeatAt = now;
  }
  if (uint32_t(now-logAt) >= 3000) { logAt = now; status(); }
  delay(1);
}
