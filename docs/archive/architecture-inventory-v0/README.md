# Architecture Inventory v0 归档摘要

> `status`: `archived`

## 归档原因

旧 `architecture_overview.md` 长期累积目录清单、模块广告、UI 能力回收待办、第三方依赖、POSIX/ModuleX/FS 图、测试状态和当前 focus，已经无法作为稳定实现地图。现行入口改为：

- [`../../architecture_overview.md`](../../architecture_overview.md)

## 保留的讨论价值

- `Foundation / Runtime / Domains` 曾用于提醒依赖向上和领域层不得反向渗透；现在只保留为实现分区启发，不作为 Core 语义。
- 静态能力走 `init.graph`，运行期发现设备走 discovery/export，两者不能混用。
- UI/Audio 等领域代码应复用公共 IO、trace、容器和时间能力，但复用必须由真实消费者证明，不能靠“大一统”口号。
- 聚合入口、叶子模块和退役 facade 需要明确区分。
- Host、QEMU、MCU 示例应形成分级证据，而不是汇总成一句“全部已通过”。

## 不作为现行事实

- UI/Ink 与 UI/Vivid 的能力回收清单属于历史迁移计划，不是全局架构契约。
- SDL3、FatFs、dr_libs 的开关列表属于构建/专题文档。
- Kernel、FS、ModuleX、Shell 的 Mermaid 图是当时快照，容易随 import 漂移。
- `Windows M0-M3 已通过`、`STM32 编译通过` 和具体 Vivid focus 已过期且没有同页证据。
- “所有公开 API 都必须使用 `util::Result`”“Channel 只能 non-blocking”等 blanket 规则超出实际覆盖范围，应由具体接口契约约束。
- “允许短期主动耦合”不再作为默认原则；任何例外都必须在具体边界记录理由和退出条件。

## 真实板与能力回收

- [`real_board_landing_gap_audit_v0.md`](real_board_landing_gap_audit_v0.md)：保留早期 stm32h747-player
  的并行 console、私有诊断、默认接线和规则工程化反例；其中项目路径已经失效；
- [`rk3506_boot_staging_plan.md`](rk3506_boot_staging_plan.md)：保留 RK3506 SDK 启动线索、
  vendor DDR/SPL 依据和历史阶段映射；其中地址与 vendor loader 判断是历史分析，
  不能替代 TRM、当前源码或实板证据。

板级 workaround 不能自动进入公共契约；Host、QEMU 和真实板证据必须分开记录。

现行入口：

- [`../../architecture/real_board_landing_gap_audit_v0.md`](../../architecture/real_board_landing_gap_audit_v0.md)
- [`../../board/rk3506/boot_staging_plan.md`](../../board/rk3506/boot_staging_plan.md)

不要把本摘要恢复成第二份全局架构定义。
