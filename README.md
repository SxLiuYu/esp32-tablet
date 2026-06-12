# ESP32 Tablet - yuanfang-brain Voice Frontend

## 硬件配置

| 外设 | 型号/参数 | ESP32 GPIO |
|------|-----------|------------|
| SPI LCD | 1.54" ST7789 240x240 | MOSI=23, CLK=18, CS=5, DC=21, RST=22 |
| 麦克风 | I2S INMP441 | BCK=26, WS=25, DIN=33 |
| 扬声器 | I2S MAX98357A | BCK=26, WS=25, DIN=33 (shared I2S) |
| 按键 | VOL-/POW/BOOT/RGB/VOL+ | 35/0/39/27/32 |
| RGB 灯条 | WS2812B x8 | GPIO13 |
| 电池 ADC | 分压电阻 100k+100k | GPIO34 |
| 4G 扩展 |艾尔赛科技 CZ8767 | UART (未用) |

> 注意: 经典 ESP32 (WROOM-32, 520KB SRAM, **无 PSRAM**) — 固件已针对低内存优化

## 协议

连接 `ws://192.168.1.10:7103/ws/audio` (yuanfang-brain WS 服务器)

**发送音频:**
```json
{"type":"audio","data":"<base64 16kHz 16bit PCM>","sr":16000}
```

**接收回复:**
```json
{"type":"reply","text":"回复文本","audio":"<base64 MP3>","intent":{...}}
```

## 目录结构

```
esp32-tablet/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   ├── main.c          # 入口, 状态机
│   ├── wifi.c/h        # STA + SmartConfig fallback
│   ├── ws_client.c/h   # WebSocket 客户端 (yuanfang-brain 协议)
│   ├── audio.c/h       # I2S MIC 采集 + MAX98357A 播放
│   ├── display.c/h     # ST7789 SPI + LVGL 渲染 (半屏 fb)
│   ├── keypad.c/h      # 5 按键 GPIO 处理
│   ├── rgb.c/h         # WS2812B RMT 驱动
│   ├── power.c/h       # 电池 ADC + 深度睡眠
│   └── ui.c/h          # LVGL UI 状态显示
└── README.md
```

## 编译

### 环境要求
- ESP-IDF v5.4.2
- 经典 ESP32 工具链 (xtensa-esp32-elf)

### 本地编译
```bash
cd ~/repos/esp32-tablet
source $IDF_PATH/export.sh
idf.py set-target esp32
idf.py build
```

### Docker 编译 (无本地 IDF)
```bash
docker run --rm -v ~/repos/esp32-tablet:/project \
  espressif/idf:release-v5.4.2 \
  sh -c "cd /project && source /opt/esp-idf/export.sh && idf.py set-target esp32 && idf.py build"
```

## 烧录

```bash
# 1. 查看串口
ls /dev/tty.usbserial-*

# 2. 擦除并烧录 (替换 /dev/tty.usbserial-* 为实际串口)
idf.py -p /dev/tty.usbserial-* erase_flash

# 3. 烧录固件
idf.py -p /dev/tty.usbserial-* -b 921600 flash

# 4. 烧录后查看日志
idf.py -p /dev/tty.usbserial-* monitor

# 5. 退出 monitor: Ctrl+]
```

**esptool.py 手动烧录:**
```bash
esptool.py --chip esp32 --port /dev/tty.usbserial-* erase_flash
esptool.py --chip esp32 --port /dev/tty.usbserial-* \
  --baud 921600 write_flash 0x1000 \
  build/bootloader/bootloader.bin 0x10000 build/esp32-tablet.bin \
  0x110000 build/partitions.bin
```

## yuanfang-brain 服务启停 (Mac 192.168.1.10)

```bash
# 启动 ws_server (后台)
cd ~/repos/yuanfang-brain && nohup python -m voice.ws_server > ws.log 2>&1 &

# 确认运行
lsof -i :7103

# 停止
pkill -f "voice.ws_server"
```

## 真机测试 Checklist

- [ ] 烧录后蓝色 LED 闪一下 → 启动成功
- [ ] 绿色 LED → WiFi 连接成功
- [ ] 红色 LED → WiFi 连接失败, 进入 AP 模式
- [ ] 按 BOOT 键 → 蓝色 LED + 屏幕 "Listening..." → 说话
- [ ] 回复音频播放 + 屏幕绿色文字显示回复
- [ ] RGB 键 → 彩灯渐变演示
- [ ] 电池电量显示正常 (BAT: XX%)
- [ ] `idf.py monitor` 无 panic/error

## WiFi 配网 (首次)

固件中 SSID/PASSWORD 为占位符, 需替换为真实 WiFi:

```bash
# 方法 1: 直接修改 wifi.c 第 36-37 行
#   cfg.sta.ssid  = "你的WiFi名";
#   cfg.sta.password = "你的密码";

# 方法 2: 使用 nvs_partition_gen.py 生成默认 WiFi
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \
  --inputkey wifi_config \
  --output wifi_config.bin --size 0x5000
```

## 经典 ESP32 内存风险提示

⚠️ **无 PSRAM**, 以下为实测风险点:

| 区域 | 大小 | 风险 |
|------|------|------|
| LVGL 半屏 framebuffer | 57.6KB | ✅ 已在内部 DRAM |
| WS 音频队列 (4x8KB) | 32KB | ⚠️ 触发时注意 |
| audio capture task 栈 | 4KB | ✅ 够用 |
| LVGL task 栈 | 4KB | ⚠️ 若不够可调至 5KB |
| ws_resp_buf | 16KB | ⚠️ 若收到大 JSON 可能溢出 |

**建议:** 量产前用 `heap_caps_print_all()` 确认剩余 > 80KB

## 许可证

MIT