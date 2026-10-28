# 上游组件

项目源码通过下载脚本使用以下官方依赖，下载目录不提交到仓库，使用时保留每个组件的许可证：

| 组件 | 固定版本或提交 | 上游 |
|---|---|---|
| STM32L4 HAL | 047a8610c7dc0dcfdb4cd63b86cd3a2cbd3e5899 | https://github.com/STMicroelectronics/stm32l4xx-hal-driver |
| STM32L4 CMSIS Device | dc04e41afbd7c204cd09f2fa5dd29658efbaff6c | https://github.com/STMicroelectronics/cmsis-device-l4 |
| CMSIS Core | 5.9.0 | https://github.com/ARM-software/CMSIS_5 |

HAL 和 Device 提交来自 STM32CubeL4 v1.18.2。具体许可文本以各组件源文件和仓库内 LICENSE 为准。

构建工具使用 GNU Arm Embedded 工具链，兼容 Arm 官方发行版或 xPack 分发版。Windows 本地验证使用 xPack GCC 14.2.1，工具不包含在仓库中。主机 C 测试支持 GCC / Clang / TinyCC，测试编译器不属于固件运行时。
