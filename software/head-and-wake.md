# 摇头与遥控唤醒（2026-11-04）

CURRENT_STATE.md优先。用户最终确认摇头电机已经接在原电机板；不用另接小智GPIO。C路此前叫“躯干”，本次明确作为曲轴摇头电机：电机板G6/G7→原H桥CIN1/CIN2。左右履带仍是15/16和4/5。

## 操作

- 遥控器首页独立中文“摇头”键：按住持续摇头，松开停止。曲轴持续转动即产生摇头，默认单向满占空比；底层支持正反转并保留120ms换向停顿。没有新增方向键占用。
- “唤醒”键：点一下发事件到电机板，经既有G13/G14交叉UART转发给小智。它调用已有WakeWordInvoke：待机唤醒；说话时打断；监听中点击关闭当前音频会话。600ms按键去重，按住不会连发。
- 语音板须先完成配网和服务初始化才能真正语音对话。此次用户选择稍后配网；日志可能只显示REMOTE_WAKE received，不会进入对话。摇头和履带遥控不依赖小智联网。
- 单触摸策略；多触摸、触控异常超过120ms、进入设置/配对、断线或关机时停止摇头请求。接收端250ms租期独立于履带，超时/重新配对后必须收到中立帧才能再转。

## 代码与协议

- motor/Coco/HeadState.h：独立输出、租期、中立重置、换向保护；MotorDriver C路不再固定0。
- ESP-NOW仍48字节、信道6、CRC32、原MAC/session/nonce/递增sequence校验。新增Kind::Head=8，steering=0，throttle=0或±1000，lease≤250ms；50ms连续发送。Head/Dialogue只更新共享sequence，不续履带租期/连接心跳，方向控制仍独立50ms。
- Kind::Dialogue=7新增steering=3/throttle=0表示唤醒。与指定/下一段/停止共用单待发槽，断线清空。原预置对话接口保留，此次没有安排对话按钮。
- Offer发现包用steering携带发送端工作信道（1～11）；旧设备steering0时仍回退接收元数据。实机遇到接收端报告5而电机实际6，明确广播信道避免选错。配对处理回调后重新读取当前millis，再计算超时；旧代码的旧now减刚写入pairStarted会无符号下溢，导致首次请求未发就立即超时。
- 反向UART仍24字节，magic0x444f4343；WAKE=0、throttle=steering=0，绑定当前语音session/sequence。与运动magic分离，不能作为电机Drive。
- source/voice仅改板型自己的coco_link.h及反向协议头，调用已有主循环调度入口，不改唤醒词/音频GPIO/云传输契约。
- 遥控器双手模式：摇头x232/y88/w112/h96，唤醒x352/y88/w112/h96；单手模式：摇头x324/y88/w140/h72，唤醒x324/y192/w140/h72。保持四个原方向触摸矩形。
- 中文位图在coco_button_labels.h；仅两组字，运行无需字体库。再生成工具见tools/render_remote_button_labels.ps1，需要Windows微软雅黑。

USB诊断：遥控器115200发送`REMOTE WAKE`测试同一待发事件；电机板115200发送`WAKE`测试电机→语音部分。不生成摇头动作。`STATUS`可看HEAD target/output/armed/active。运动必须观察实物，协议测试不替代摇头实测。

## 更新范围

三端必须配套更新。旧电机固件固定C路0且不认识Head/唤醒事件，单刷遥控器/小智不能启用摇头。只更新各自应用分区（语音0x20000，电机/遥控0x10000），保留配网、引脚映射和原配对；刷写前按MAC核对并备份前64KB配置区域。精确烧录和实测结论以debug/head_wake_validation.md为准。
