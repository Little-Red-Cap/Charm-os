# Early Diagnostics 早期取舍保留笔记

> status: `archived`
>
> scope: 复杂系统装配完成前的最小诊断问题，不定义 Foundation Runtime 或统一 App model

早期提案曾试图把 log、panic、time、identity、Board/Platform/System/Scenario 分层和应用上下文统一
放进 `Foundation Runtime`。这个概念过宽，缺少稳定消费者与跨环境证据，不应进入 Charm Core。

## 真实问题

系统在 graph、scheduler、reactor、USB、UI 或 storage 完整装配前就可能失败。如果第一条诊断依赖
这些高层能力，启动故障会变成“越需要观察，越无法输出”。这要求项目明确一个更早的诊断路径，但
不要求建立新的全局 runtime object。

## 局部责任

Platform/BSP 或具体 execution environment 可以提供最小 early sink，例如 UART polling、semihosting、
host stderr 或已有 boot console。该边界应明确：

- 初始化前置条件和最早可调用阶段；
- 是否允许 IRQ、锁、分配、格式化或阻塞；
- buffer 满、设备未就绪和递归 fault 时的行为；
- panic/fault 路径是否要求 best effort、flush 或 reset；
- full diagnostics 可用后如何切换、复用或停用 early sink。

最小 monotonic tick、reset reason、build identity 或 region facts 只有在真实消费者需要且该阶段可可靠
提供时才加入，不能作为“基础运行时”默认套餐。

## 应用入口边界

App 或 Scenario 入口不应拥有 startup、clock、UART pinmux、vector table 或 board bring-up 顺序。进入
应用前，平台应明确哪些依赖已经成立；未成立的 capability 不能靠一个宽泛 `AppContext` 假装存在。

统一应用入口必须来自真实 App ABI 或 capability contract，而不是为了传递 log/time/identity 就创建
不断扩张的 context bag。Host 和 MCU 可以投影相同诊断行为，但 backend、最早阶段和失败能力不同。

## 不能证明的事

- early log 可见不证明 scheduler、graph 或应用已健康；
- host stderr 可用不证明 MCU fault path 可重入或可刷新；
- UART polling 可用不证明 IRQ/DMA console 已接管；
- build/version string 不等于稳定 product identity contract；
- 两个平台都有 `print()` 不足以证明需要公共 Foundation Runtime。

## 重新推进条件

只有出现至少两个 execution environment 的真实消费者，并能固定调用阶段、最小操作、失败行为、
交接策略与独立 smoke，才考虑提升为 Stable Boundary。实现应先留在 Project/BSP 或局部 provider 中。

旧提案的四层 Runtime、Player/USB 推演、统一 context 伪 API 和落地排期已删除。
