## SmartLamp
## 主要硬件与依赖

- ESPC3-12-N4 芯片（理论兼容 ESP32-C3 系列）
- 1.54寸 240x240 TFT 屏（ST7789 驱动）
- WS2812B RGB 灯珠 *64
- PlatformIO（推荐使用 VSCode 插件）
- 框架：Arduino for ESP32

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
	- 支持 Wi‑Fi 管理（连接、重连、配置加载）
	- 支持 MQTT 与 Home Assistant 集成，状态上报与命令订阅
	- 配置与凭据本地持久化，主题与发现机制可配置

- **蓝牙接口**
	- 提供 BLE 命令通道，用于本地配置、固件触发与简易控制

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
