# IO 分层总览（HAL/Platform/Input/FS/USB/Net）

相关决策文档：
- `docs/input_layering_decision.md`（UI/Ink 输入层分层决策）


## 目标

明确 IO 能力在 Charm 架构中的位置与依赖方向，避免“领域语义下沉”和“底层反向依赖”。

## 总体分层（沿用 Foundation -> Runtime -> Domains）

- Foundation：纯能力、无平台、无生命周期
  - core/util
  - core/service
  - out/*
  - trace_core

- Runtime：系统运行期与硬件交互层
  - system/kernel
  - io/hal
  - io/fs
  - io/usb
  - io/net
  - platform/*

- Domains：完整领域系统（UI/Audio/Shell/…）
  - ui/ink
  - ui/vivid
  - media/audio
  - shell

## IO 运行时分层（建议命名）

```
io/
  hal/            # 纯硬件抽象接口（统一 API + 最小语义）
  driver/         # 厂商/系列驱动实现（可选单独目录）
  service/        # 运行期适配：fs/net/usb/input 等
platform/
  board_caps/     # 板级能力描述（clock/irq/uart/...）
```

- hal：对上提供“稳定接口”，对下由 driver 实现
- driver：芯片级外设实现（可按 vendor/series 组织）
- service：对上领域友好，包含状态机/缓存/协议适配

## 能力边界与职责

### HAL（io/hal）
- 只暴露“硬件能力”与“最小行为语义”
- 不包含 UI/Audio/FS 的业务语义
- 可绑定 Rust/其他语言接口

### Platform（platform/*）
- 只做平台差异与最底层绑定
- 允许使用寄存器、IRQ、时钟、DMA、内存布局

### Driver（io/driver）
- 按厂商/系列/芯片组织
- 实现 HAL 接口，保持接口一致性

### Service（io/service）
- 以运行期语义封装 HAL
- 示例：
  - input 服务：RawInputEvent -> UI intent
  - fs 服务：块设备 -> VFS
  - usb/net 服务：端点/协议适配

## 依赖规则（硬约束）

- Foundation 不依赖 Runtime/Domains
- Runtime 可依赖 Foundation，但不得依赖 Domains
- Domains 可依赖 Foundation 与 Runtime
- hal 不依赖 domain 语义，不引入 UI/Audio/FS 逻辑

## 与 VSF HAL 的对齐（参考点）

VSF 在 `Draft/vsf/source/hal` 中采用：

- arch：架构相关底层
- driver：厂商/芯片驱动
- common/template：统一外设接口模板
- template：HW/IPCore/Emulated 三类驱动模板

启示：

- HAL 以统一接口为中心，而非平台细节
- driver 与 template 形成“接口一致性约束”
- Rust 绑定（bindgen）依赖 hal/driver/driver.h

## 输入层的定位（简版）

- RawInputEvent 应落在 Runtime（io/hal 或 io/service/input）
- UI intent/gesture 只在 Domains/UI
- 队列/背压策略：Runtime 只负责投递与统计

## 后续落地建议

1. IO 目录结构在文档中固定
2. 为每个 IO 子域建立“接口 + 参考实现 + stub”三件套
3. 以 VSF 模板思想约束 HAL 接口一致性

---

备注：本文件是架构边界定义，不替代各模块设计文档。




