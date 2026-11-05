# 接口与二进制协议

CURRENT_STATE.md优先。字节精确定义以当前头文件和宿主测试为准；以下为当前ESP32小端、packed结构。

## UART1：115200 8N1

两板TX13/RX14交叉，3.3V共地。正向运动头文件：source/motor/Coco/coco_uart_protocol.h；语音端有对应副本，改动须同步并测。

24字节：magic:u32, session:u32, sequence:u32, throttle:i16, steering:i16, kind:u8, reserved[3], crc:u32。保留位必须0；session非0；轴范围-1000～1000。CRC32覆盖前20字节，多项式0xedb88320、初值0xffffffff、最终取反；测试向量123456789→0xcbf43926。

- 运动magic=0x314f4343；ACK magic=0x414f4343。
- Kind Idle0 / Drive1 / Stop2 / Rejected3。
- 新会话必须先Idle/Stop；递增序号用有符号差处理回绕；重复/旧序号拒收。
- 50ms发帧；有效Drive到期250ms停止；Idle不更新Drive接收时间，Stop显式停；保留120ms换向停顿和遥控优先。
- MCP self.robot.move(direction,duration_ms)：100～2000ms，默认500ms；self.robot.stop()。

## 反向对话事件

coco_dialogue_protocol.h复用24字节封装，独立magic=0x444f4343。PLAY1：throttle为1～16槽号；NEXT2/STOP3：throttle=0；steering=0；绑定当前语音会话并检查递增序号/CRC。

电机板仅在语音心跳最近500ms有效时转发，使用最近已接收语音sequence，单语音sequence最多一事件。反向事件独立解析，不能变成运动命令；密集触发可能被去重，不能依赖无限队列。

## 遥控 ESP-NOW

source/motor/Coco/WallEProtocol.h与source/remote/WallERemote/WallEProtocol.h应保持相同。固定信道6；48字节：magic:u32,version:u8,kind:u8,bytes:u16,sequence:u32,session:u32,nonce:u32,throttle:i16,steering:i16,batteryMv:u16,leaseMs:u16,name[16],crc:u32。magic=0x57414c45/version1；CRC覆盖前44字节。

Kind Discover1/Offer2/PairRequest3/PairAck4/Control5/Heartbeat6/**Dialogue7**。控制周期50ms，租期250ms，机器人心跳200ms，遥控失联1000ms。保留配对MAC/session/nonce/新序号检查。原ESP-NOW不加密；CRC不是安全认证。

Dialogue7：steering 0=指定槽（throttle1～16），1=下一段（throttle0），2=停止（throttle0）。只有有效配对会话、lease≤250、队列包龄<lease、递增sequence且参数有效的包才转发；更新sequence但**不延长Drive租期**。注意遥控steering动作编码和反向UART kind编码不同。

## USB串口命令

语音115200：COCO STATUS；COCO MIC GAIN 1..16；COCO WAKE（软件诊断）；COCO DIALOG 1..16 / NEXT / STOP；COCO TEST FORWARD|BACKWARD|LEFT|RIGHT 100..2000；COCO STOP。

电机115200：DIALOG 1..16 / NEXT / STOP；新空板映射PINS 15 16 4 5 6 7后REBOOT。完整电机控制台命令见Coco.ino；不要对已有板随意重置NVS/配对。

## 2026-11-04扩展（高于上文旧范围）

ESP-NOW可解码kind范围更新为1～8，新增Head8：steering0、throttle0/±1000、lease≤250ms、50ms发送，独立C路租期。Dialogue7增加steering3/throttle0表示唤醒。已配对MAC/session/nonce/新序号检查保留，新辅助包仅更新sequence，不延长履带租期或连接心跳。

反向UART增加WAKE0，throttle=steering=0，其他PLAY1/NEXT2/STOP3不变；coco_dialogue魔数与运动魔数分开。底层运动种类没有扩展。细节见head-and-wake.md；三端配套更新。
