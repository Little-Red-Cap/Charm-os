# IO 分层总览（HAL/Platform/Input/FS/USB/Net）

相关决策文档：
- `docs/architecture/driver_model.md`
- `docs/input/input_layering_decision.md`（UI/Ink 输入层分层决策）

这份文档负责说明 IO 在仓库里的主题边界与分层位置，更接近“总览入口”，不是 `io.channel / io.reactor / io.registry` 的硬契约正文。

如果这里的表述与现行规则冲突，优先以：

- `docs/architecture/driver_model.md`
- `docs/io/io_channel_contract.md`
- `docs/io/io_reactor_contract.md`
- `docs/io/io_registry_contract.md`
- 当前代码

为准。

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

- hal：对上提供“稳定接口”，对下由 controller backend 实现
- driver：更适合放控制器后端实现与控制器级适配
- service：对上领域友好，包含状态机、缓存、协议适配，是 ServiceAdapter 的主要落点

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
- 更接近控制器后端，而不是默认等同于 runtime discovery plane 的 `device::Driver`

### Service（io/service）
- 以运行期语义封装 HAL
- 示例：
  - input 服务：RawInputEvent -> UI intent
  - fs 服务：块设备 -> VFS
  - usb/net 服务：端点/协议适配
- 在驱动模型文档里，这一层对应 `ServiceAdapter`
- 对普通调用方，优先暴露 service capability / registry handle，而不是 `BoardCaps` 或 runtime driver hook

## 推荐驱动路径

对于板级已知资源，推荐理解为：

```text
BoardCaps
  -> ControllerBinding
  -> ServiceAdapter
  -> registry / capability export
```

对于运行期枚举设备，推荐理解为：

```text
RuntimeBus
  -> discovered device
  -> RuntimeDriver
  -> capability export
```

两条线最终都应收敛到统一 capability / registry 语言，避免形成两套互不相通的资源模型。

对用户侧的默认要求是：

- 优先消费 `io.*` / `block.*` / service facade
- 不要求理解 `BoardCaps` / `DeviceDesc` / `match_score`
- 不直接围绕 `RuntimeBus` / `RuntimeDriver` 编程

## 依赖规则（硬约束）

- Foundation 不依赖 Runtime/Domains
- Runtime 可依赖 Foundation，但不得依赖 Domains
- Domains 可依赖 Foundation 与 Runtime
- hal 不依赖 domain 语义，不引入 UI/Audio/FS 逻辑

## 与 VSF HAL 的对齐（历史参考点）

VSF 的 `source/hal` 目录采用：

- arch：架构相关底层
- driver：厂商/芯片驱动
- common/template：统一外设接口模板
- template：HW/IPCore/Emulated 三类驱动模板

启示：

- HAL 以统一接口为中心，而非平台细节
- driver 与 template 形成“接口一致性约束”
- Rust 绑定（bindgen）依赖 hal/driver/driver.h

如果需要继续查看 Charm 侧保留的历史对照入口，优先从：

- `docs/reference/vsf/README.md`

进入，而不是依赖已经废弃的本地工作目录写法。

## 输入层的定位（简版）

- RawInputEvent 应落在 Runtime（io/hal 或 io/service/input）
- UI intent/gesture 只在 Domains/UI
- 队列/背压策略：Runtime 只负责投递与统计

## 后续落地方向

1. IO 目录结构在文档中固定
2. 为每个 IO 子域建立“接口 + 参考实现 + stub”三件套
3. 以 VSF 模板思想约束 HAL 接口一致性

---

备注：本文件是主题边界总览，不替代各模块设计文档或 IO 核心契约正文。





