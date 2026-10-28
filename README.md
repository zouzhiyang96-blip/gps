# 超长待机定位器固件原型

基于 **STM32L476RGT6 + Quectel EC200U** 的最小通信任务工程，代码位于 [firmware](firmware/README.md)。

本版实现初始化、采样、一次 TCP 上报、业务确认、失败记录保存、命令处理占位和 RTC / 传感器唤醒。采用新设计的引脚及测试协议，**不是可直接刷入 LL702 的原厂兼容固件**。

- [工程说明与构建](firmware/README.md)
- [引脚与供电连接](firmware/docs/hardware.md)
- [通信协议及指令扩展](firmware/docs/protocol.md)
- [首次联调与已知限制](firmware/docs/bringup.md)

请先阅读硬件连接要求，修改本地 APN 与服务器配置，再编译和烧录。真实硬件上的供电时序、网络通信和休眠电流仍需实测。
