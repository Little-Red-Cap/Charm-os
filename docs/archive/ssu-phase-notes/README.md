# SSU 阶段材料摘要

## 文档状态

- `status`: `archived`
- `scope`: 早期 SSU 推进记录、迁移取舍和未决问题
- `current contract`: [`../../system/ssu_contract.md`](../../system/ssu_contract.md)

本页保留早期 SSU 推进中的证据、迁移取舍和未决问题；阶段计划与当时状态不作为当前事实。

## 当时记录的证据

阶段材料曾记录：

- `system.reactor_pump`、`input.pump`、`canopen.pump` 已声明不同的 SSU 元数据；
- Scheduler 已提供 event、io-ready、demand 三类提交来源统计；
- `TaskRegistry` 严格模式曾在 Player CM7、独立 Charm-dap 和
  `Examples/kernel/rtos/qemu` 等目标上做过构建尝试；
- RunLoop 已增加强类型 `SubmitProjection` 和 JSON 审计输出；
- scheduler export 已增加 overview、hotspots 和 event source 输出。

这些是历史推进记录，不是当前构建通过的证明。使用前应从现有源码、CMake target 和最新测试
重新验证。

## 保留的迁移取舍

- 优先审查 pump、reactor drain、service/protocol progress 等容易形成内部推进旁路的路径。
- RunLoop 和 UI phase 只适合在局部证据充分后继续映射，不应因标签相同就重构为同一模型。
- 设备时钟、DMA/IRQ 和低抖动数据面应暂缓统一；先保留上下文和 defer 边界。
- 新旁路需要记录原因、范围、退出条件和回收路径。
- 先建立可观测和可审查的入口，再评估是否值得改变调度行为。

## 未解决的问题

- `Meta` 的描述如何与真实阻塞、预算和上下文行为核对；
- 严格模式应在哪些 target 默认启用；
- submit 来源统计和 hotspots 阈值是否足以支持排障；
- RunLoop projection 是否值得进入更强的契约；
- audio 或其它设备时钟数据面是否需要完全不同的执行模型。
