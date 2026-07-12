# 真实板落地缺口审计

## 文档状态

- `status`: `supporting`
- `scope`: 从真实板证据识别能力、接缝与默认路径问题
- `authority`: 受 [`charm_core_contract.md`](charm_core_contract.md) 约束

本文提供审计方法，不记录某块板的当前状态，也不为局部实现补发 Charm Core 准入资格。

## 五类问题

真实板暴露的问题必须先分类：

1. **能力缺失**：没有满足消费方行为、错误和不变量的实现；
2. **不可发现**：实现存在，但现行入口无法让使用者找到；
3. **接缝阻力**：实现可见，但接入成本使项目自然退回私有路径；
4. **不是默认路径**：已有可行接线，但模板、profile 或样例仍选择另一条路径；
5. **规则未工程化**：文档写了“推荐”，构建、模板和失败检查却没有执行。

“项目选择了私有实现”不能直接证明 Charm 缺能力；“仓库存在同名组件”也不能证明能力已经可用。

## 稳定边界

### EvidenceRig

真实板项目可以作为 EvidenceRig 暴露时序、cache、外设和装配问题。它提供反例和运行证据，
但其目录、HAL 接线、shell 风格或 workaround 不自动成为跨平台契约。

### Early 与 Runtime 路径

pre-graph、fault 和极早期生存证据可以直连 port/UART。系统建立正常 console/binding 后，运行期
业务仍使用早期路径必须有明确理由和退出条件。例外可命名，不得伪装成默认接口。

### Read-only Snapshot

共享协调层在接管重 runtime 前，应先消费无副作用、可重复读取的 status/snapshot。snapshot 能打印
不等于它已经成为 Capability Contract；只有真实消费方、错误语义和跨环境证据齐备后才能申请提升。

## 审计证据

每个结论至少记录：

- 当前消费方真正依赖的行为；
- 现有 source/CMake/profile 中的实现与默认接线；
- 私有路径为什么被选择；
- 缺失、重复、阻力或例外的可复现证据；
- 修复后的编译检查与一个行为证据。

Host、QEMU 和真实板是不同证据域。Host metadata、schema、README 或 build-only 不能替代真实板运行。

## 与 Core 的关系

`DefaultConsolePath`、`EarlyConsole`、`ServiceSnapshotContract` 与 `EvidenceRig` 是历史审计中的局部术语，
不属于 Constitution 已批准的 Core vocabulary。现行工程规则见
[`capability_recovery_rules.md`](capability_recovery_rules.md)。

旧 H747 路径、完整问题清单和 P0-P4 排期见
[`../archive/architecture-inventory-v0/real_board_landing_gap_audit_v0.md`](../archive/architecture-inventory-v0/real_board_landing_gap_audit_v0.md)。
