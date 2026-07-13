# Device Interface v0 保留说明

> **状态：`archived`**
>
> 本文保留五份早期 device interface 提案的未决语义，不定义 Charm Core、公共 ABI 或已实现能力。
> 当前事实从 [`architecture/README.md`](../../architecture/README.md) 进入。

## 共同边界

- backend/controller handle、endpoint/device ref 和上层 policy 必须分层；基础接口不拥有 vendor handle、
  filesystem、UI intent 或 scheduler policy。
- 同步函数形状不自动表示 non-blocking、ISR-safe、reentrant、线程安全或有 deadline。
- timeout、等待、重试和 async progression 由 timebase/reactor/runtime 显式承接，不能隐藏在基础操作中。
- endpoint 发布、backend 存活和真实介质/目标可用是不同状态。
- candidate error taxonomy 只有在两个 backend/consumer 和正反 smoke 证明后才值得进入公共接口。
- facts/evidence 是只读 sidecar；producer 名称或 artifact report 不能证明设备存在。

## SPI

带片选的 consumer 若需要 managed transaction，CS assert/deassert、必要 flush 和 bus serialization 应由
device/transaction boundary 持有，不能复制到每个 driver。早期提案没有冻结 transaction API，也没有
证明同步 transfer 的 TX/RX 长度、半双工、全双工、DMA、timeout 或并发语义。

需要区分的失败至少包括 bus/mode/overrun、chip-select、detach 与 unsupported；是否保留专用分类取决于
未来 mock 和真实 consumer，不能只为一个 backend 扩展公共错误枚举。

## GPIO

input level、output level 与 edge occurrence 是三种职责。edge backend/ISR 只应把 occurrence 送入明确
ingress；debounce、repeat、click、long-press 和 UI intent 属于 sampler/input service 或更高层。

未决失败包括 invalid pin、direction mismatch、not configured、detach 和 backend IO。active polarity、
pull、drive strength 与 safe default 是 board/product 选择，不是统一 pin ABI。

## Block

backend、对外 endpoint 与 removable media state 必须分开。geometry、range、alignment、read/write/erase
granularity 和 flush 含义需要由具体 media/backend 说明；filesystem、partition 与 cache replacement policy
不进入 block operation contract。

调用必须明确成功处理的 block 数或完整失败，不能用未说明的 partial operation。候选失败包括 invalid
geometry、out of range、read/write/erase/flush fault、media missing、detach 与 write protect。

## Stream IO

一次成功 read/write 必须推进至少一个 byte；暂不可推进返回 `would_block`。short read/write 可以是成功，
但 caller 必须看实际长度。缺失 flush 应报告 `not_supported`，不能伪装成成功零长度操作。

closed、detached、would-block 和 backend fault 必须可区分。reactor/EDA 负责等待与通知，基础 stream
不 busy-spin、自建 timeout loop 或在 ISR 中执行完整 consumer。

## Timebase

最小 timebase 是只读时间源，需要说明 unit、resolution、width、monotonic、wrap 与可调用上下文。
读取时间不等于 sleep、timeout dispatch、periodic timer、managed/replay time 或 wall clock。

未绑定返回零会混淆 missing 与真实零时刻，是现有 `Clock` 形状的已知缺口。timeout-aware consumer 在
依赖 timebase 前还需声明最低 resolution 和 wrap 处理策略。

旧正文中的拟议 C++ API、facts 全量清单、`proposed -> experimental` gate、producer 命名、阶段排期和
next steps 已删除。需要追溯原文时使用 Git 历史。
