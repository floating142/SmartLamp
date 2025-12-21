#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void) {
    {  // ---- SPI 总线配置 ----
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;   // ESP32-C3 使用 FSPI (SPI2_HOST)
      cfg.spi_mode = 3;           // SPI 模式 0 或 3
      cfg.freq_write = 80000000;  // [优化] 提升至 80MHz 以获得更高帧率 (原 40MHz)
      cfg.freq_read = 20000000;   // 读取频率通常较低
      cfg.spi_3wire = true;       // ST7789 通常使用 3 线 SPI (MOSI 兼作数据线)
      cfg.use_lock = true;        // 使用事务锁
      cfg.dma_channel = SPI_DMA_CH_AUTO; // 自动分配 DMA 通道

      cfg.pin_sclk = 3;   // SCLK
      cfg.pin_mosi = 4;   // MOSI
      cfg.pin_miso = -1;  // ST7789 通常没有 MISO
      cfg.pin_dc = 2;     // DC (Data/Command)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {  // ---- 屏幕面板配置 ----
      auto cfg = _panel_instance.config();
      cfg.pin_cs = -1;    // 如果 CS 接地或未使用，设为 -1
      cfg.pin_rst = 5;    // 复位引脚
      cfg.pin_busy = -1;  // 忙信号引脚

      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.memory_width = 240;     // [优化] 显式指定显存宽度
      cfg.memory_height = 240;    // [优化] 显式指定显存高度
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.invert = true;          // ST7789 IPS 屏通常需要反色
      cfg.rgb_order = false;      // 像素顺序 RGB 或 BGR
      cfg.dlen_16bit = false;     // 数据位宽 8位
      cfg.bus_shared = true;      // 总线是否共享

      _panel_instance.config(cfg);
    }
    {  // ---- 背光配置 ----
      auto cfg = _light_instance.config();
      cfg.pin_bl = 1;             // 背光引脚
      cfg.invert = false;         // 背光是否反相
      cfg.freq = 5000;           // [优化] 5kHz PWM 频率，避免人耳听到的啸叫
      cfg.pwm_channel = 0;        // 使用 PWM 通道 0
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};
