# Coco 双 ESP32-S3 语音机器人

本仓库保存 Coco 的有效源码、固件、硬件接线、设计决策和验收边界，供没有看过历史聊天的新对话继续开发。语音板运行定制小智，电机板管理履带和遥控优先级；遥控器通过 ESP-NOW 控制电机板。

**最高优先级：[CURRENT_STATE.md](CURRENT_STATE.md)。** 任何 README、上游板型、归档、记忆或历史日志与它冲突时，采用 CURRENT_STATE.md。硬件连接状态必须现场核实，不能把历史验收当作实时在线状态。

## 项目目标

- 自然说“嘿扣扣”唤醒，先播本地“我在”，按连接状态提示服务器连接，然后普通语音对话。
- 保留遥控控制和两板串口电机控制，100% 履带输出，并保持超时停止等保护。
- 演示时用遥控器触发固定回答；当前仅对话1有音频，按钮只预留接口。
- 将当前源码和必要记录集中到这里；双麦暂缓。

## 架构

```text
INMP441 → 语音 S3 ↔ Wi-Fi / 小智云（MQTT+UDP 实机路径）
              ↓ MAX98357 类功放 → 扬声器
       UART1 TX13/RX14 ↔ RX14/TX13 电机 S3 → H桥 → 左右履带
                                      ↕ ESP-NOW / 信道6
                              WallE 触摸遥控器
```

## 在另一台电脑上传

完整ZIP包含`.git`本地提交历史；解压后按[UPLOAD_TO_GITHUB.md](UPLOAD_TO_GITHUB.md)用GitHub Desktop发布为私有仓库，或使用已登录的GitHub CLI。没有远端仓库或上传成功的预设。

## 新对话先读

1. CURRENT_STATE.md：有效状态、DO NOT USE、未确认项。
2. TODO.md：下一步及阻塞；DECISIONS.md：选择和冲突依据。
3. hardware/wiring.md、software/setup.md、debug/verified_tests.md。
4. 再按改动范围读对应源码；archive/ 和 source/voice 内通用上游文档不是部署状态。

## 文件用途

| 路径 | 内容 |
|---|---|
| CURRENT_STATE.md | 唯一有效状态，优先级最高 |
| TODO.md / DECISIONS.md / DEPRECATED.md | 工作清单、最终决策、已废弃方案 |
| hardware/ | BOM、接线、引脚、供电已知事实与缺口 |
| software/ | 架构、二进制协议、构建和烧录 |
| debug/ | 已知问题、实测结论、冲突核对记录 |
| source/voice/ | 小智 2.5.2 定制 coco-voice-uart 工程，含锁定依赖 |
| source/motor/Coco/ | 电机 Arduino 工程及宿主协议测试 |
| source/remote/ | 遥控工程和厂家配置库；对话接口预留版 |
| firmware/ | 2026-10-30 构建产物；遥控产物尚未刷入 |
| tools/ | 当前音色生成、校验工具 |
| Flash-Device.ps1 / SHA256.json | 按 MAC 核对、备份、校验后烧录；交付文件哈希 |
| manifests/ | 本次迁移的文件哈希与来源记录 |
| archive/ | 仍有参考价值的历史交付说明，低优先级 |

没有 Wi-Fi 密码、服务器令牌、NVS、个人语音录音或聊天原文。保留上游和厂商许可证；没有为第三方源码重新授权。源码中的其他板型和测试夹具是构建/参考内容，不代表本机配置。

第三方依赖包含原样保留的MQTT TLS测试密钥/证书和音效示例PCM，它们是输入交付包中的上游测试夹具，不是用户设备凭据或用户录音；路径和精确哈希见manifests/vendor-test-fixtures.json。未联网验证其上游公开状态，不可用于实际服务认证。隐私扫描按已核对哈希允许这些夹具，其他凭据形状仍阻止。保留它们以避免手改锁定依赖。
