# Telegram Desktop 机制比较保留笔记

> status: `archived`
>
> scope: 外部大型产品机制带来的审查问题，不证明当前 Telegram 或 Charm 实现

本文是历史比较，不是上游源码导读。Telegram Desktop 的具体库、API 与仓库结构可能已经变化；需要
引用上游事实时必须重新检查其源码和许可。Charm 不因外部项目采用某机制就应复制它。

## 生命周期归属

Reactive pipeline 的关键不是 operator 语法，而是订阅、callback 和 target 由谁取消。嵌入式实现需
额外回答固定容量、取消时机、并发 emit、对象销毁和 pending delivery。

Charm 当前的 delegate/signal/state 是局部、同步、non-owning 原语，不自动等价于 RPL，也不提供通用
producer graph。若未来增加 producer/value，必须先证明真实消费者和 lifetime owner。

## Execution Domain

跨线程、ISR、task、reactor、UI 或 audio callback 的切换应显式。一个 `post()` façade 不足以统一：

- 是否允许阻塞或分配；
- 调用是否 IRQ-safe；
- queue capacity、drop 和 cancellation；
- 时间源与 timer 语义；
- shutdown 与 target lifetime。

不同 backend 可以投影相似调用形状，但 execution environment 的约束不能被“主线程抽象”抹平。

## Style 与尺寸归属

视觉尺寸、间距、字体和颜色若散落在 widget 逻辑中，会使主题、density 和 backend 降级不可审查。
token/table 可以集中这些事实，但不应因此建立一个控制 layout、render、animation 和产品设计的全局
style runtime。单位、fallback、memory cost 和编译产物仍需由具体 UI contract 说明。

## Schema 单一事实源

协议、descriptor 或持久化布局需要唯一 source of truth；C++ type、validator、plan 和 runtime 是投影。
但不是所有配置都需要 DSL/codegen。只有存在跨语言/版本消费者、wire compatibility 或重复手写漂移时，
schema 才可能比局部 typed API 更合适。

`spec -> validate -> plan -> runtime` 是可选处理链，不是 Charm Core 世界模型。生成器不能重新定义领域
语义，也不能用 schema 存在证明 runtime 行为。

## 平台差异隔离

平台目录和 adapter 的价值是限制 startup、MMIO、SDK、window system、threading 和 filesystem 差异的
传播。它们无法消灭差异，也不应建立全局 Platform 基类。上层只依赖真实 Capability Contract；BSP、
backend 和 execution environment 仍保持各自 ownership。

## Persistent Storage Evolution

顺序二进制、TLV、KV、日志或数据库需要不同演进规则。通用审查问题是：

- magic/version/length/integrity 如何验证；
- 新字段、未知字段和缺失字段如何处理；
- interrupted write、rollback 和迁移失败如何恢复；
- endian、alignment、wear 与容量上限由谁负责；
- reader/writer compatibility 是否有 fixture。

“字段只能 append”只适用于某些顺序格式，不应提升为所有 storage 的全局法律。

## 注释解释原因

普通控制流不需要逐行旁白。应记录的是寄存器时序、cache/DMA coherency、barrier、ABI、errata、协议
例外、不变量、失败恢复和临时妥协的退出条件。注释不能替代 source/test，也不能把未来方案写成事实。

## 不从案例推导

- 不因桌面产品使用动态 reactive library，就为 MCU 建立缩小版 Rx；
- 不因外部项目拆 submodule，就预设 Charm 仓库拆分方案；
- 不因 schema/codegen 成功，就把所有配置升级为 schema；
- 不因 platform adapter 存在，就宣称平台差异已经统一；
- 不把外部协作文件直接复制为 Charm Core 规则。
