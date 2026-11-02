# Coco voice UART

Independent ESP32-S3 board variant based on bread-compact-wifi, without an OLED.

- MAX98357 audio: DIN GPIO7, BCLK GPIO15, LRC GPIO16.
- Voice UART1: TX GPIO13, RX GPIO14, 115200 8N1, 3.3V logic.
- Motor UART1: TX GPIO13, RX GPIO14. Cross TX to RX and share GND.
- Microphone remains the original single-mic I2S configuration. Dual-mic audio has not been enabled.
- MCP tools: `self.robot.move(direction, duration_ms)` and `self.robot.stop()`.
- Actions last 100–2000 ms, default 500 ms. Receiver stops after 250 ms without valid movement frames.
- Valid remote movement takes priority over UART movement. Idle UART packets do not stop remote movement.
- Local console test, without a microphone: `COCO TEST FORWARD 300`, `COCO TEST BACKWARD 300`, `COCO TEST LEFT 300`, `COCO TEST RIGHT 300`, `COCO STOP`.
- Console tests cause physical motion. Suspend the treads before testing. The motor firmware retains the user's 100% duty setting.
- Custom MultiNet wake phrase: `hei kou kou` (嘿扣扣 / 嘿Coco), default threshold 20%.
- Single INMP441: WS GPIO4, SCK GPIO5, SD GPIO6, L/R to GND, 3.3V supply. Keep the verified 5cm leads.
- Local current-voice prompts and presentation replies are in `sounds/`; `coco_presets.h` maps slots to exact recorded text.
- USB commands: `COCO DIALOG 1`, `COCO DIALOG NEXT`, `COCO DIALOG STOP`. Repeated requests restart playback; the latest pending request wins.
- Wake and microphone upload are disabled during preset playback and restored on completion. No new remote UI buttons are assigned yet.
- `COCO STATUS` exposes input peak/clipping counters. `COCO WAKE` tests the software path only, not microphone recognition.
- Keep Wi-Fi in performance mode and omit wake-audio upload to reduce response overhead. TLS validation remains enabled; cloud delays remain variable.
- NoDisplay must skip visual resource creation in Assets::Apply, while keeping model and speech asset loading enabled.
- `CocoAudioCodec` defaults to 4x input gain instead of the common codec's fixed 16x. `COCO MIC GAIN 1..16` adjusts it until reboot; input peak and accumulated clipping are reported by `COCO STATUS`.
- A persistent MQTT control connection counts as already connected for feedback: open the short-lived audio channel without the long connecting announcement. Local cues gate microphone upload, while listen/start is sent promptly to avoid server inactivity closure.

Build with ESP-IDF 6.1: `python scripts/build.py coco-voice-uart --name coco-voice-uart`.
Flash only after confirming the voice ESP32 MAC is 84:C7:BB:5A:AE:DC. The motor board MAC is 84:C7:BB:58:E7:88.
