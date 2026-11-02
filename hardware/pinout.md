# 引脚定义与源码位置

CURRENT_STATE.md优先；所有引脚分板命名，不能混用。

| 板 | 用途 | GPIO | 依据 |
|---|---|---|---|
| 语音 | 麦克风WS/SCK/DIN | 4/5/6 | source/voice/main/boards/coco-voice-uart/config.h |
| 语音 | 功放DOUT/BCLK/LRCK | 7/15/16 | 同上 |
| 语音 | UART1 TX/RX | 13/14 | COCO_UART_TX/RX；coco_link.h |
| 语音 | BOOT / 板载LED | 0/48 | config.h定义；额外实体接线未确认 |
| 电机 | 左AIN1/2 | 15/16 | 已存NVS映射和实测；Coco.ino PINS命令 |
| 电机 | 右BIN1/2 | 4/5 | 同上 |
| 电机 | 躯干CIN1/2 | 6/7 | 已配置，未验收运动 |
| 电机 | UART1 TX/RX | 13/14 | Coco.ino Serial1.begin(115200,SERIAL_8N1,14,13) |

语音config.h仍带显示SDA41/SCL42及其他通用按钮/灯常量；不等于本机有接线或GUI启用。遥控屏幕/触摸脚保持source/remote中厂家配置，以实际库User_Setup为准，禁止换默认TFT_eSPI配置。

## 源码默认值不等于有效映射

MotorConfig.h的DEFAULT_PINS={4,5,6,7,8,9}仅示例；Coco.ino必须读取已保存PINS才允许驱动。当前实际NVS映射为15,16,4,5,6,7，不能按示例数组重接线。MotorDriver假定外部PWM/STBY已拉高，但实物驱动型号和每根使能线接法未确认。INVERT_A=false、INVERT_B=true；PWM10kHz/8bit，MAX_DUTY=START_DUTY=255，保留RAMP/120ms换向停顿。START_BOOST_MS=100仍在结构中，但起步/上限均255，不代表当前是旧boost临时固件。
