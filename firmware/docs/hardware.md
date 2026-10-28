# 硬件假设与引脚

这是重新选择的 STM32L476RGT6 参考连接，适合配合 NUCLEO-L476RG 和外部模块验证。**与 LL702 主板引脚没有兼容性承诺**。只有核实原板芯片、线路、供电和可恢复下载入口后，才可考虑移植。

## 引脚分配

| STM32 引脚 | 用途 | 配置与外部连接 |
|---|---|---|
| PA9 | USART1 TX | AF7，115200 8N1，经隔离/电平转换接模块 MAIN_RXD |
| PA10 | USART1 RX | AF7，115200 8N1，经电平转换接模块 MAIN_TXD |
| PB0 | MODEM_POWER_EN | 推挽，高有效，控制外部通信电源负载开关 |
| PB1 | MODEM_PWRKEY | 高有效驱动外部 NPN/NMOS，导通时拉低模块 PWRKEY |
| PA0 | BATTERY_ADC | ADC1_IN5，接可关断的 200 kΩ / 100 kΩ 分压中点 |
| PC0 | BATTERY_SENSE_EN | 推挽，高有效，控制分压支路的外部模拟/负载开关 |
| PB12 | SENSOR_INT | 上升沿 EXTI，内部下拉，外部高电平唤醒 |
| PA2 / PA3 | 调试 USART2 | AF7，115200 8N1，默认不启用，可接 ST-LINK 虚拟串口 |
| PA5 | 调试 LED | 上电与休眠时保持低，不作常亮状态灯 |
| PC14 / PC15 | LSE | 32.768 kHz 晶体电路，失败后退回较低精度 LSI |
| PA13 / PA14 | SWDIO / SWCLK | 保留 ST-LINK 调试下载 |
| PB10 / PB11 | 预留 GNSS 串口 | USART3 TX/RX AF7，当前保持模拟输入，不初始化 |
| PB2 | 预留 GNSS_POWER_EN | 低电平关闭，当前不启动 GNSS |
| PB6 / PB7 | 预留传感器总线 | I2C1 SCL/SDA AF4，当前保持模拟输入，不初始化 |

PB12 是外部传感器或外接测试按键输入，不使用 NUCLEO 板载 B1 按键。真实运动传感器的极性和中断保持方式需与此配置匹配，首版未加入复杂报警门限及去抖策略。

## 电源和电平

- STM32 假设稳压 3.3 V 供电。EC200U 按模块硬件手册设计独立电源及储能，不由 GPIO 或普通开发板 3.3 V 引脚供电。
- EC200U 裸模块主串口属于 1.8 V 域。使用适合 UART 的 3.3 V / 1.8 V 电平转换，确认转换器断电隔离能力，OE 应与模块电源状态协调。不要把 3.3 V TX 直接连接到裸模块 RX。
- PB0 和 PB1 均需要外部下拉，使 MCU 复位期间通信电源保持关闭、PWRKEY 驱动释放。PWRKEY 使用晶体管开漏拉低，不能把 PB1 高电平直接加到模块 PWRKEY。
- 首次调试建议使用带正确供电、电平转换和天线接口的 EC200U 开发板，并核对它是否自动开机。代码假设模块受本固件控制，**不适用于未改造的自动开机接法**。
- VBAT 电压测量假设 VDDA=3.300 V、分压比例为 3、滤波电容 10 nF。PC0 使能支路后等待 5 ms 再采样。实际 VDDA、分压误差和门限需校准；本版没有剩余容量百分比模型。
- 程序给电源约 2.1 秒稳定时间，再拉低 PWRKEY 约 2.1 秒，之后进行 AT 就绪检测。关机先发 QPOWD，最长预留 30 秒，超时才执行外部断电作为故障退路。

## Flash 和 RAM

| 地址 | 大小 | 用途 |
|---|---:|---|
| 0x08000000-0x080BFFFF | 768 KiB | 首版应用和向量表 |
| 0x080C0000-0x080FDFFF | 248 KiB | 命令表与将来的 `.command_handlers` 扩展代码 |
| 0x080FE000-0x080FEFFF | 4 KiB | 将来参数配置区，本版不用 |
| 0x080FF000-0x080FF7FF | 2 KiB | 状态副本 A |
| 0x080FF800-0x080FFFFF | 2 KiB | 状态副本 B |
| 0x20000000 起 | 96 KiB | SRAM1，首版数据、堆和栈 |
| 0x10000000 起 | 32 KiB | SRAM2，首版保留 |

代码检查 1 MiB Flash、双 Bank 和未交换 Bank 模式后才运行，避免按错误擦除几何写入状态页。**不自动修改 Option Bytes**。两页状态每次只擦除非活动副本，最后写入提交双字。启动时校验格式、CRC 和提交标记，选择最新有效副本。

当前每条新记录和确认清除各提交一次，冷启动也提交启动次数。这是双页演示存储，频繁上报会消耗 Flash 寿命。量产前应按擦写寿命和上报频率评估，必要时使用日志轮转或外部耐久存储。整片擦除会丢失序号和待发记录，同一 UID 的序号重新开始可能与服务器旧记录冲突，需要明确设备重置/重新登记流程。

## STOP2 边界

进入休眠前关闭模块、UART 和采样支路，挂起 SysTick，检查待处理唤醒位，再以原子检查和 WFI 进入 STOP2。唤醒使用同一 MSI 4 MHz 时钟，无 PLL 恢复。HAL 毫秒计时只用于活动任务，不用来表示休眠中的真实日期时间。

RTC 定时器在任务结束后设置，唤醒后停用。RTC 与传感器同时触发时唤醒原因按位合并。调试器、ST-LINK、电平转换器和电源板仍可能耗电，不能把 MCU 数据手册中的休眠电流当作整机实测值。

## 官方参考

- [STM32L476RG 产品与数据手册](https://www.st.com/en/microcontrollers-microprocessors/stm32l476rg.html)
- [ST RTC 唤醒 STOP2 说明](https://community.st.com/t5/stm32-mcus/how-to-configure-the-rtc-to-wake-up-the-stm32-from-low-power/ta-p/49836)
- [EC200U Hardware Design V1.2](https://www.quectel.com/content/uploads/2024/02/Quectel_EC200U_Series_Hardware_Design_V1.2.pdf)
