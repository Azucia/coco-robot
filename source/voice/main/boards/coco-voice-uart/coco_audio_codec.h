#pragma once
#include <esp_log.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include "codecs/no_audio_codec.h"

// INMP441 data is left aligned in a signed 32-bit I2S slot. Keep a configurable
// gain instead of the generic codec's fixed 16x amplification, which clipped
// this microphone during the measured wake-word test.
class CocoAudioCodec : public NoAudioCodecSimplex {
public:
    using NoAudioCodecSimplex::NoAudioCodecSimplex;
    void SetInputGain(float gain) override {
        if (!std::isfinite(gain))
            return;
        const int value = std::clamp(int(gain), 1, 16);
        gain_.store(value);
        ESP_LOGI("CocoAudio", "Microphone gain=%d", value);
    }

protected:
    int Read(int16_t* dest, int samples) override {
        buffer_.resize(samples);
        size_t bytes = 0;
        if (i2s_channel_read(rx_handle_, buffer_.data(), samples * sizeof(int32_t), &bytes, 200) !=
            ESP_OK)
            return 0;
        const int gain = gain_.load();
        samples = bytes / sizeof(int32_t);
        for (int i = 0; i < samples; ++i) {
            const int64_t value = (int64_t(buffer_[i]) * gain) >> 16;
            dest[i] = int16_t(std::clamp(value, int64_t(INT16_MIN), int64_t(INT16_MAX)));
        }
        return samples;
    }

private:
    std::atomic<int> gain_{4};
    std::vector<int32_t> buffer_;
};
