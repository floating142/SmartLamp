## SmartLamp

[![PlatformIO](https://img.shields.io/badge/PlatformIO-PlatformIO-2ea44f?logo=platformio&logoColor=white)](https://platformio.org/) [![LVGL](https://img.shields.io/badge/LVGL-LVGL-3AA3E6?logo=lvgl&logoColor=white)](https://lvgl.io/) [![Espressif ESP32](https://img.shields.io/badge/Espressif-ESP32-ff4d00?logo=espressif&logoColor=white)](https://www.espressif.com/) [![Arduino](https://img.shields.io/badge/Arduino-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/) [![FastLED](https://img.shields.io/badge/FastLED-FastLED-FF7A59?logo=fastled&logoColor=white)](https://github.com/FastLED/FastLED)

## 开发环境
- PlatformIO（VSCode）
- 框架：Arduino
- 开发板：esp32-c3-devkitc-02
- 分区表：`partitions.csv`

## 主要硬件
- ESPC3-12-N4 模块（ESP32‑C3 系列）
- WS2812B（16×4 灯板，支持色温与亮度控制）
- 1.54" ST7789 TFT（240×240），显示使用 LovyanGFX + lvgl
- BH1750 环境光传感器
- DHT40 温湿度传感器（或 SHT4x 可选）
- CW2015 电量检测 IC
- DS3231 RTC（实时时钟）
- 18650 电池 + 充放电管理（IP5407 / IP3005A）
- 触摸（TTP223）与拨轮输入（ADC）
- 人体存在传感器 HLK‑LD2410D

## 目录结构
```
src/           # 主固件源码（按功能模块划分）
	app/         # 灯效与核心逻辑
	sensors/     # 各类传感器驱动与管理
	network/     # Wi-Fi、MQTT、BLE 等网络相关
	ui/          # 本地 UI 与显示
	input/       # 按键输入
	system/      # I2C、RTC、存储等系统服务
data/          # 静态资源（如图片）
include/, lib/ # 头文件与第三方库
platformio.ini # PlatformIO 配置
```

## 主要功能
- **智能灯控制**
	- 多种灯效引擎（淡入淡出、效果序列、渲染流水等）
	- 亮度与色彩控制（支持 RGB/HSV 处理与色温调整）
	- 支持状态与配置持久化

- **传感器采集**
	- 支持 BH1750（光照）、SHT4x（温湿度）、CW2015（电量/充放电状态）等多种传感器
	- 统一管理，支持轮询、数据过滤与事件分发

- **简易雷达驱动（未完成）**
	- 支持 LD2410D 雷达的基础通信与数据解析
	- 目前仅实现原始数据读取，目标检测/跟踪与高级控制尚未实现

- **网络与集成**
	- 支持 Wi‑Fi 管理（连接、重连、配置加载）；采用自动重连与指数退避策略以降低资源占用
	- MQTT：
		- 话题约定：`<base>/state`（发布状态）、`<base>/set`（接收命令）、`<base>/availability`（在线/离线）
		- 支持 LWT、QoS=1（可靠投递）与 retained（重要状态保持）
		- `device_info` 与 MQTT Discovery 支持，便于 Home Assistant 自动注册实体
		- TLS 可选（受限 SRAM 下谨慎启用）；客户端ID 唯一化并带时间戳/设备ID以便识别
		- 处理建议：限制单条消息大小（避免大 JSON），使用心跳/重连策略并记录可用性
	- 与 Home Assistant 集成：使用 MQTT Discovery，上报 `device_info`、属性与 availability；保持话题一致性便于自动化
	- 配置与凭据本地持久化，主题、QoS 与发现机制可在配置中调整

- **蓝牙接口**
	- 提供 BLE 命令通道（基于 NimBLE）：
		- 角色与用途：设备作 Peripheral，主要用于配网（写入 Wi‑Fi 凭据）、本地控制命令、状态订阅
		- GATT 设计：分为 Provisioning Service（写入/确认 Wi‑Fi）、Control Service（命令/状态特征），建议将大载荷分块传输以节省内存
		- 性能与内存：限制最大并发连接数、适当设置 MTU、减少广告/扫描频率以降低 RAM 与 CPU 占用
		- 使用建议：BLE 作为配网与本地紧急控制通道，常规状态同步与自动化仍依赖 MQTT

- **本地 UI 与输入**
	- 本地显示与 GUI，支持多种屏幕渲染
	- 支持 GPIO 按键与 ADC 按键，短按/长按及多键逻辑

- **系统服务**
	- I2C 总线统一管理，支持多外设接入
	- RTC 定时任务与时间戳
	- 持久化存储，支持分区管理

- **模块化架构**
	- 代码按功能模块划分，每个模块独立实现任务与接口
	- 模块间通过消息队列/事件回调解耦，便于扩展与维护

## 注意事项（内存与 Flash）

- ESP32‑C3 性能有限，只有单核，且只有 400 KB SRAM，容易内存溢出，需小心优化；需特别注意 FreeRTOS 系统任务堆栈。
- 程序应尽量精简并控制内存峰值，避免同时分配大堆与大缓冲。此程序实现了 WiFi 与 BLE 共存，导致开机后内存仅剩 29 KB。
- 通过网页（HTTP）获取 JSON 会占用大量内存，应避免一次性大缓冲或并发解析，优先使用流式/分块解析并复用缓冲区。
- C3 内置 flash 仅 4 MB，空间有限；已将程序分区调整为 3 MB（需权衡 OTA 与数据存储空间）。

## 主要依赖库及来源

- [FastLED](https://github.com/FastLED/FastLED)
- [Sensirion I2C SHT4x](https://github.com/Sensirion/arduino-i2c-sht4x)
- [BH1750](https://github.com/claws/BH1750)
- [DS3231](https://github.com/northernwidget/DS3231)
- [lvgl](https://github.com/lvgl/lvgl)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
