# 构建、检查与刷写

先读CURRENT_STATE.md和各源码目录AGENTS.md。当前工程含锁定依赖；SDK、编译器和私有设备配置不在仓库。下列构建在2026-10-30通过，迁移未重建；全新机器从零复现未确认。

## 语音板

启用ESP-IDF6.1（最低6.0.1，不能5.x）的开发环境，从仓库根目录：

```powershell
cd source/voice
$env:CMAKE_NINJA_FORCE_RESPONSE_FILE='1'
python scripts/build.py coco-voice-uart --name coco-voice-uart --language zh-CN
```

Windows长Ninja命令发生CreateProcess失败时使用上述response-file选项。不要加--wake-word nihaoxiaozhi。自定义词config.json须和固件/资源共同重建；build.py会改本地sdkconfig。依赖见dependencies.lock、idf_component.yml和managed_components；不要手改生成/厂商内容。

## Arduino

Arduino ESP32 core 3.3.10-cn（当时安装来源/镜像可持续性未确认；不是默认为官方同号核心）。本地曾用Arduino_Complete_2026-10-15中的arduino-cli，工具路径不作为仓库依赖。

```powershell
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashMode=dio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi' --output-dir firmware/motor source/motor/Coco
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=dio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi' --libraries source/remote/libraries --output-dir firmware/remote source/remote/WallERemote
```

上面是迁移后等效相对路径命令，尚未在新克隆环境执行；当时两工程构建通过。保留厂家TFT_eSPI User_Setup和FT6336，勿替换默认屏幕配置。修改产物后更新哈希才使用烧录脚本，不能让旧清单验证新固件。

## 烧录

关闭串口监视器，读取MAC确认：voice 84:C7:BB:5A:AE:DC；motor 84:C7:BB:58:E7:88。COM仅现场选择。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./Flash-Device.ps1 -Target voice -Port COM13
powershell -NoProfile -ExecutionPolicy Bypass -File ./Flash-Device.ps1 -Target motor -Port COMxx
```

COM13只是历史示例。脚本校验SHA256.json、MAC，读前64KB到忽略的private-backups，再分段写入；不发运动命令。默认查本机Arduino esptool，可用-EsptoolPath指定。不能提交备份，不能擅自全片擦除。

语音DIO/80MHz/16MB：0x0 bootloader，0x8000 partition-table，0xd000 ota_data_initial，0x20000 xiaozhi，0x800000 generated_assets；资源约2.99MB/8MB分区。电机原板仅应用0x10000，保留映射和配对；空板首次配置完整布局须另核实。不要把旧091V3的0x100000用到这里。遥控版当前不提供自动刷写默认流程，等按钮确定并核对身份。

## 台词与唤醒

改source/voice/main/boards/coco-voice-uart/coco_presets.h和sounds/对应Ogg，文字音频一起替换；槽2～16未配置。tools/generate_current_voice.py经自己的已绑定小智服务生成，内存持有令牌；tools/requirements.txt列依赖，Windows Huihui Desktop用于生成请求，输出使用服务器当前音色。

```powershell
python -m pip install -r tools/requirements.txt
python tools/generate_current_voice.py --device-id 84:c7:bb:5a:ae:dc --client-id <设备启动日志中的UUID>
```

尖括号为占位，替换后运行。修改tools/coco_speech_lines.json；已有同名ogg默认跳过，先单独备份移走待替换文件；回复文字不符会拒绝打包。当前泛化工具仅语法核对，原本地生成流程已实测，泛化版未另发请求复测。修改后重建资源和固件。不包含token、Wi-Fi密码、NVS或设备绑定绕过。

## 检查

```powershell
python tools/audit_repository.py
python -m unittest discover -s source/voice/scripts/tests -v
# 在可用C++编译环境中：
c++ -std=c++17 -I source/motor/Coco source/motor/tests/protocol_test.cpp -o protocol-test
```

最后运行生成的测试可执行文件。Windows当时用Zig0.13.0的c++编译协议测试；宿主上游适配器见tools/run_host_tests_windows.py，测试跳过必须明示。不要把构建/软件注入当物理验收。运动测试COCO TEST FORWARD 300前先确认H桥上电、临时线撤除、履带悬空，准备COCO STOP并观察实际停止。

仓库Git本地提交的自动整理作者为Coco Project Migration <coco-migration@localhost>，不是推断出的用户身份。新对话不依赖此作者名辨识硬件。第三方自带属性可能规范少量文本换行，精确允许值见manifests/line-ending-normalization.json。
