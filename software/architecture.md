# 软件架构

CURRENT_STATE.md优先。source/是当前有效代码，firmware/是2026-10-30匹配产物；第三方依赖保留原许可证和锁定文件。

## 语音端（ESP-IDF）

- main/application.*：主事件循环、唤醒流程、本地播放状态与云对话协调。回调通过Schedule/event bits修改应用；不阻塞主线程/音频任务。
- main/boards/coco-voice-uart/compact_wifi_board.cc：独立板型、NoDisplay、串口链路、预置表、性能模式和本地反馈资源。
- coco_audio_codec.h：仅Coco重写输入32位PCM转16位，默认4倍，限幅及可调增益。
- coco_link.h：50ms UART帧、ACK、运动/MCP、USB诊断、反向对话事件。
- coco_presets.h / sounds/：槽位1～16表，仅1有缓存；四个Ogg为我在、连接提示、组合提示和回答1。
- main/notify/notify_player.*：asset://映射Ogg异步有界解码，取消与播放ID避免过期回调；保留HTTP播放路径。
- main/assets.cc：先加载语音模型，无GUI跳过LVGL字体/图片/主题。
- main/protocols/：通用IsTransportConnected；MQTT控制通道连接时不重复长提示，WebSocket走通用音频连接判断。
- main/audio/audio_service.*：峰值和累计削波诊断；期间停麦/停唤醒避免自身音频干扰。

自定义板构建链：config.json → scripts/build.py → Kconfig.projbuild → main/CMakeLists.txt → 唯一DECLARE_BOARD。核心代码不得包含具体板config.h。阅读source/voice/AGENTS.md后再修改。

## 预置回答流程

遥控预留API → ESP-NOW Kind7 → 电机板鉴权/序号校验 → 反向UART对话事件 → CocoPresets → Application本地播放。电机板USB DIALOG命令的后半链路已实测；遥控触摸入口尚未实现。

新请求取消旧生产者，等其退出、清理旧解码，再启动新播放；最新请求覆盖等待槽。关闭云音频通道并屏蔽云音频/相关回调，暂停麦克风和唤醒，完成后回idle。台词文字和Ogg必须一起改。

## 电机端（Arduino）

Coco.ino拥有UART/ESP-NOW、NVS映射与配对；CocoState.h/DriveState等保留遥控会话、neutral rearm和换向保护；MotorDriver.h输出。有效非零遥控覆盖UART，结束后不恢复已经抑制的旧语音动作。对话事件不延长运动租期，也不进入Drive解析器。

## 遥控端（Arduino）

source/remote/WallERemote，现有UI保留。remote_app.h预留remotePlayDialogue(1..16)、remotePlayNextDialogue()、remoteStopDialogue()。返回true仅表示排入本地单槽，非机器人播放ACK。断线/重连清空待发对话；方向心跳优先于对话包。带摇头/唤醒键的新固件已于11/4刷入；预置对话按钮仍未安排。

## 2026-11-04控制扩展

MotorDriver C路改为HeadState独立租期输出，不再固定停止。遥控单触摸驱动头部保持帧，唤醒单次事件沿用电机板中转UART到板型CocoLink，调用已有WakeWordInvoke主循环调度。没有核心音频/网络协议改动；自定义词和GPIO保留。具体接口和未验收部分见head-and-wake.md。
