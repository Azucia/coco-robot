# 实际验证记录

基线2026-10-30；整理2026-11-02。这里是历史验收，迁移本身没有再次触发硬件。CURRENT_STATE.md优先。

| 测试 | 结果 / 证据 | 边界 |
|---|---|---|
| 两板刷写 | voice 84:C7:BB:5A:AE:DC、motor 84:C7:BB:58:E7:88，esptool写入校验 | 当时COM13/COM20，不能当当前端口 |
| 三工程编译 | IDF6.1语音；Arduino3.3.10-cn电机/遥控均通过 | 遥控未刷入，新克隆重建未测 |
| NoDisplay启动 | 模型、Wi-Fi/MQTT、待机、对话工作；未再现LVGL分配崩溃 | 其他板型无实机回归 |
| 资源 | 约2.99MB，校验和/模型命令hei kou kou/阈值20%/4个Ogg核对通过，音频可解码 | 修改后必须重新打包 |
| 自然唤醒 | Custom wake word detected ... hei kou kou；用户“ok 嘿coco连接没问题” | 不能用COCO WAKE冒充自然唤醒 |
| 连接耗时 | 曾151/181ms；最终软件触发路径179ms | 仅音频通道握手，不含识别/推理/TTS；非整体响应承诺 |
| 本地回答1 | 我来自烟台理工学院；重复触发从头重播用户确认 | 只有槽1有内容 |
| 播放隔离 | notifying状态7/wake=0，完成idle状态3/wake=1 | 不代表长期所有网络竞态均覆盖 |
| UART | ack=1 age_ms=50 rejected=0 | 当时测量 |
| 反向对话 | 电机USB DIALOG 1→语音正确播出，电机输出0 | 尚非遥控实际触摸入口 |
| 履带短测 | COCO TEST FORWARD 300；左右PWM255；UART_STOP all_IN=0后0；用户“两边都动了，并停住” | 躯干未动作；语音MCP链未专门实测 |
| 协议宿主 | CRC、会话、旧序号、250ms租期、编号范围、对话不驱动/不续运动租期通过 | 见source/motor/tests/protocol_test.cpp |
| Ogg/格式 | 解复用宿主测试、改动C++格式检查通过 | 不是所有上游测试通过 |
| 上游宿主 | Ran 110 tests，OK(skipped=6)，104通过 | Windows临时cwd清理适配；Bash/Git集成及c++环境跳过明确保留 |
| 原交付ZIP | 13416文件、约395.9MiB、testzip通过 | 本迁移排除重复嵌套build，目录数量不应机械相等 |

原ZIP SHA256：605c011a66fc3905a7816d60c9e01d62983e87e7c5b72df4e676569658a92fd4。

关键日志仅转为结论，不上传包含私有对话/配置的原始日志。迁移的文件完整性、重复协议和链接检查结果在debug/migration_audit.md及manifests/记录；它不能替代上述实物验证。
