#pragma once
#include <esp_log.h>
#include "application.h"

class CocoPresets {
public:
    // Slots 2..16 are intentionally unassigned. Add the exact spoken audio to
    // sounds/ and extend this table; no button/UI choice is imposed here.
    struct Entry {
        int slot;
        const char* text;
        const char* asset;
    };
    inline static constexpr Entry entries[] = {
        {1, "我来自烟台理工学院。", "coco_dialogue_01.ogg"},
    };
    void Trigger(int slot) {
        if (slot == -1) {
            Application::GetInstance().PlayLocalAsset("");
            return;
        }
        if (slot == 0) {
            const auto& entry = entries[next_];
            next_ = (next_ + 1) % (sizeof(entries) / sizeof(entries[0]));
            Play(entry);
            return;
        }
        for (const auto& entry : entries) {
            if (entry.slot == slot) {
                Play(entry);
                return;
            }
        }
        ESP_LOGW("CocoPresets", "Dialogue slot %d has no configured audio", slot);
    }

private:
    size_t next_ = 0;
    void Play(const Entry& entry) {
        ESP_LOGI("CocoPresets", "Dialogue slot=%d text=%s", entry.slot, entry.text);
        Application::GetInstance().PlayLocalAsset(entry.asset, entry.text);
    }
};
