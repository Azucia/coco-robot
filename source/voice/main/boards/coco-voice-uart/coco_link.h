#pragma once
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <unistd.h>
#include <functional>
#include <string>
#include "application.h"
#include "coco_dialogue_protocol.h"
#include "coco_uart_protocol.h"
#include "config.h"
#include "mcp_server.h"

class CocoLink {
    static constexpr uart_port_t PORT = UART_NUM_1;
    SemaphoreHandle_t mutex_ = nullptr;
    uint32_t session_ = 0, sequence_ = 0;
    int16_t throttle_ = 0, steering_ = 0;
    int64_t until_ = 0, stop_until_ = 0;
    coco_uart::Parser parser_, event_parser_;
    coco_dialogue::Receiver event_receiver_;
    std::function<void(int)> on_dialogue_;
    uint32_t last_ack_ = 0;
    bool have_ack_ = false, last_rejected_ = false;
    char console_[64]{};
    size_t console_length_ = 0;
    static int64_t Now() { return esp_timer_get_time() / 1000; }

    void Set(int16_t throttle, int16_t steering, int duration) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        throttle_ = throttle;
        steering_ = steering;
        until_ = Now() + duration;
        if (!throttle && !steering)
            stop_until_ = Now() + 300;
        xSemaphoreGive(mutex_);
    }
    void Run() {
        uint8_t input[96];
        const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags >= 0)
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        for (;;) {
            for (unsigned n = 0; n < 64; ++n) {
                char c;
                if (read(STDIN_FILENO, &c, 1) != 1)
                    break;
                if (c == '\r')
                    continue;
                if (c == '\n') {
                    console_[console_length_] = 0;
                    if (!strcmp(console_, "COCO STATUS")) {
                        ESP_LOGI("CocoLink", "COCO_LINK ack=%u age_ms=%lu rejected=%u", have_ack_,
                                 static_cast<unsigned long>(uint32_t(Now()) - last_ack_),
                                 last_rejected_);
                        auto& app = Application::GetInstance();
                        auto& audio = app.GetAudioService();
                        ESP_LOGI("CocoLink", "VOICE state=%d wake=%d peak=%d clips=%lu",
                                 int(app.GetDeviceState()), audio.IsWakeWordRunning(),
                                 audio.GetInputPeak(), (unsigned long)audio.GetInputClipCount());
                    } else if (!strcmp(console_, "COCO WAKE")) {
                        // Diagnostic injection only; this does not verify microphone detection.
                        Application::GetInstance().WakeWordInvoke("嘿Coco");
                    } else if (!strncmp(console_, "COCO MIC GAIN ", 14)) {
                        int gain = 0;
                        char extra;
                        if (sscanf(console_, "COCO MIC GAIN %d %c", &gain, &extra) == 1 &&
                            gain >= 1 && gain <= 16)
                            Board::GetInstance().GetAudioCodec()->SetInputGain(gain);
                    } else if (!strcmp(console_, "COCO DIALOG NEXT")) {
                        if (on_dialogue_)
                            on_dialogue_(0);
                    } else if (!strcmp(console_, "COCO DIALOG STOP")) {
                        if (on_dialogue_)
                            on_dialogue_(-1);
                    } else if (!strncmp(console_, "COCO DIALOG ", 12)) {
                        int slot = 0;
                        char extra;
                        if (sscanf(console_, "COCO DIALOG %d %c", &slot, &extra) == 1 &&
                            slot >= 1 && slot <= coco_dialogue::MAX_SLOT && on_dialogue_)
                            on_dialogue_(slot);
                    } else if (!strcmp(console_, "COCO STOP"))
                        Set(0, 0, 0);
                    else {
                        char direction[16]{}, extra;
                        int duration = 0;
                        if (sscanf(console_, "COCO TEST %15s %d %c", direction, &duration,
                                   &extra) == 2 &&
                            duration >= 100 && duration <= 2000) {
                            if (!strcmp(direction, "FORWARD"))
                                Set(1000, 0, duration);
                            else if (!strcmp(direction, "BACKWARD"))
                                Set(-1000, 0, duration);
                            else if (!strcmp(direction, "LEFT"))
                                Set(0, -1000, duration);
                            else if (!strcmp(direction, "RIGHT"))
                                Set(0, 1000, duration);
                        }
                    }
                    console_length_ = 0;
                } else if (console_length_ < sizeof(console_) - 1)
                    console_[console_length_++] = c;
                else
                    console_length_ = 0;
            }
            coco_uart::Packet packet{};
            packet.magic = coco_uart::MAGIC;
            packet.session = session_;
            packet.sequence = ++sequence_;
            const int64_t now = Now();
            xSemaphoreTake(mutex_, portMAX_DELAY);
            if (until_ && now >= until_ && (throttle_ || steering_)) {
                throttle_ = steering_ = 0;
                stop_until_ = now + 300;
            }
            packet.throttle = throttle_;
            packet.steering = steering_;
            packet.kind = (throttle_ || steering_)
                              ? coco_uart::Drive
                              : (now < stop_until_ ? coco_uart::Stop : coco_uart::Idle);
            xSemaphoreGive(mutex_);
            coco_uart::seal(packet);
            uart_write_bytes(PORT, &packet, sizeof(packet));
            const int count = uart_read_bytes(PORT, input, sizeof(input), 0);
            for (int i = 0; i < count; ++i) {
                coco_uart::Packet event{};
                if (event_parser_.feed(input[i], event, coco_dialogue::MAGIC) &&
                    event_receiver_.accept(event, session_) && on_dialogue_) {
                    on_dialogue_(event.kind == coco_dialogue::PLAY   ? event.throttle
                                 : event.kind == coco_dialogue::NEXT ? 0
                                                                     : -1);
                }
                coco_uart::Packet ack{};
                if (parser_.feed(input[i], ack, coco_uart::ACK_MAGIC) && ack.session == session_) {
                    last_ack_ = now;
                    have_ack_ = true;
                    last_rejected_ = ack.kind == coco_uart::Rejected;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

public:
    void OnDialogue(std::function<void(int)> callback) { on_dialogue_ = std::move(callback); }
    void Initialize() {
        ESP_ERROR_CHECK(coco_uart::selfTest() ? ESP_OK : ESP_FAIL);
        ESP_LOGI("CocoLink", "UART protocol self-test PASS");
        mutex_ = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(mutex_ ? ESP_OK : ESP_ERR_NO_MEM);
        session_ = esp_random();
        if (!session_)
            session_ = 1;
        uart_config_t config{};
        config.baud_rate = 115200;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        ESP_ERROR_CHECK(uart_param_config(PORT, &config));
        ESP_ERROR_CHECK(
            uart_set_pin(PORT, COCO_UART_TX, COCO_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(PORT, 1024, 0, 0, nullptr, 0));
        const auto created = xTaskCreate([](void* arg) { static_cast<CocoLink*>(arg)->Run(); },
                                         "coco_uart", 4096, this, 4, nullptr);
        ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
        auto& tools = McpServer::GetInstance();
        tools.AddTool("self.robot.move",
                      "Move Coco robot briefly. direction is forward/backward/left/right; "
                      "duration_ms is 100 to 2000. Movement stops automatically. "
                      "Use self.robot.stop for an immediate stop.",
                      PropertyList({Property("direction", kPropertyTypeString),
                                    Property("duration_ms", kPropertyTypeInteger, 500, 100, 2000)}),
                      [this](const PropertyList& props) -> ReturnValue {
                          const auto& direction = props["direction"].value<std::string>();
                          int16_t throttle = 0, steering = 0;
                          if (direction == "forward")
                              throttle = 1000;
                          else if (direction == "backward")
                              throttle = -1000;
                          else if (direction == "left")
                              steering = -1000;
                          else if (direction == "right")
                              steering = 1000;
                          else
                              return std::string("Invalid direction; no movement requested");
                          Set(throttle, steering, props["duration_ms"].value<int>());
                          return std::string("Motion queued; physical motion is not verified");
                      });
        tools.AddTool("self.robot.stop", "Immediately stop Coco robot movement.", PropertyList(),
                      [this](const PropertyList&) -> ReturnValue {
                          Set(0, 0, 0);
                          return true;
                      });
        ESP_LOGI("CocoLink", "UART1 TX=13 RX=14 baud=115200; bounded motion enabled");
    }
};
