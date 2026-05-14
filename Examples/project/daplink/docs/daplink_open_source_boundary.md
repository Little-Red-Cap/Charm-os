# DAPLink 开源边界说明

这份说明用于回答两个问题：

1. `Examples/project/daplink` 里哪些内容已经可以视为 DAPLink 自己的资产。
2. 哪些内容仍然属于第三方、厂商或可选集成依赖。

## 结论

当前这条线适合按“完整 STM32 DAPLink 实现”对外整理，而不是只把上层 `CMSIS-DAP` 协议部分单独拿出去。

原因很直接：

- 设备模型在这里
- USB 最小状态机在这里
- DAP transport / 调度在这里
- CDC 桥接策略在这里
- 端口分层与 STM32 backend glue 也已经在这里

也就是说，这里真正缺的不是“核心能力”，而是“独立项目口径与边界收口”。

## 项目自有代码

下面这些目录可以视为当前项目自有实现：

- `app/`
  - `CMSIS-DAP` 协议处理
  - DAP transport 与调度策略
  - CDC bridge policy
- `frontends/usb/`
  - USB 设备模型
  - 描述符生成
  - 最小控制面 / 数据面状态机
- `port/`
  - 公共契约
  - backend-neutral glue
  - board / backend 组合帮助层
- `platform/stm32/`
  - STM32 家族共用桥接层
- `base/`、`io/`
  - 当前为摆脱 `Charm` 默认依赖而内收的最小基础能力

## 第三方 / 外部依赖

下面这些内容仍然属于厂商或外部工具边界：

- 各端口目录中的 STM32Cube 生成代码
- STM32 HAL / Cube 头文件与驱动
- startup / linker script 等芯片工具链资产
- OpenOCD cfg、板卡脚本、IDE 侧调试脚本

这些内容可以随项目一起分发或保留引用关系，但不应在对外叙述里混成“项目自研部分”。

## 与 Charm 的真实关系

当前关系已经收缩到以下状态：

- 这份目录历史上来自 `Charm`
- 默认构建已经不再要求 `Charm-os`
- 仍保留可选 `Charm` 集成开关
- 当前最小基础能力已落入本目录内部，不再构成必须依赖私有仓库的硬绑定

如果只看依赖强度，可以粗略理解为：

- 架构依赖：低
- 代码依赖：低
- 构建依赖：默认低，可选集成时升高

## 当前推荐对外口径

更推荐把它表述为：

- 一个可独立构建的 `STM32 DAPLink` 项目
- 当前已经包含完整 USB 前端与 STM32 backend
- 当前首发支持范围聚焦 `STM32 family`
- 未来非 STM32 平台可沿 `platform/<vendor>/` 继续扩展

不推荐把它表述为：

- “仅提供 DAP 核心框架”
- “与 Charm 强绑定才能使用”
- “已经是一套彻底平台无关的 DAPLink 平台”
