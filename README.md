**主要功能**
- **智能灯控制**
	- 多种灯效引擎（淡入淡出、效果序列、渲染流水等）。
	- 亮度与色彩控制（支持 RGB/HSV 处理与色温调整）。
	- 关键实现文件：`src/app/lamp_core.cpp`、`lamp_effects.cpp`、`lamp_render.cpp`。
	- 状态与配置持久化：`src/app/lamp_storage.cpp`。

- **传感器采集**
	- 支持设备：BH1750（光照）、SHT4x（温湿度）、CW2015（电量/充放电状态）。
	- 驱动文件：`src/sensors/bh1750.*`、`src/sensors/sht4x.*`、`src/sensors/cw2015.*`。
	- 统一管理：`src/sensors/sensor_manager.*`，提供轮询间隔、数据过滤与事件分发。

- **简易雷达驱动（未完成）**
	- LD2410D 基础驱动位于 `src/sensors/ld2410d.*`，提供初始化与原始数据读取接口。
	- 当前仅实现基础通信与数据解析，完整的目标检测/跟踪、滤波与控制尚未实现。
	- 建议：后续添加检测算法、参数暴露与状态机实现以完成雷达控制。

- **网络与集成**
	- Wi‑Fi 管理：`src/network/wifi_task.cpp`，包含连接/重连与配置加载。
	- MQTT 与 Home Assistant 集成：`src/network/mqtt_task.cpp`、`src/network/mqtt_ha.cpp`，支持状态上报与命令订阅。
	- 配置与凭据存储在本地持久化层，主题与发现机制可配置。

- **蓝牙接口**
	- BLE 命令与任务：`src/network/ble_cmd.cpp`、`src/network/ble_task.cpp`。
	- 提供 GATT 命令通道，用于本地配置、固件触发与简易控制指令。

- **本地 UI 与输入**
	- 显示与 GUI：`src/ui/gui_task.cpp`，使用 `LGFX_ESP32` 驱动屏幕渲染（`src/ui/LGFX_ESP32.hpp`）。
	- 输入设备：GPIO 按键与 ADC 按键实现位于 `src/input`，支持短按/长按及多键逻辑。

- **系统服务**
	- I2C 总线管理：`src/system/i2c_manager.*`，为传感器与外设提供统一访问。
	- RTC 与定时任务：`src/system/rtc_task.*`，用于定时唤醒与时间戳。
	- 存储层：`src/system/storage.*`，使用 `partitions.csv` 分区进行持久化保存。

- **模块化架构**
	- 目录按功能划分：`app`、`network`、`sensors`、`ui`、`system`、`input`，每个模块独立实现任务与接口。
	- 模块通过消息队列/事件回调解耦，便于替换驱动或增加外设。

注: Flutter 客户端不包含在本仓库，请在独立项目中维护移动/桌面端。