# 真实板落地缺口审计

## 文档状态

- `status`: `supporting`
- `scope`: 从真实板证据识别能力、接缝与默认路径问题
- `authority`: 受 [`charm_core_contract.md`](charm_core_contract.md) 约束

本文提供审计方法，不记录某块板的当前状态，也不为局部实现补发 Charm Core 准入资格。

## 问题分类

| 分类 | 判断 |
|---|---|
| 能力缺失 | 没有满足 consumer 行为、错误和不变量的实现 |
| 不可发现 | 实现存在，但现行入口无法找到 |
| 接缝阻力 | 实现可见，但接入成本促使项目退回私有路径 |
| 非默认路径 | 已有可行接线，模板、profile 或样例仍选择其他路径 |
| 规则未工程化 | 文档有要求，构建、模板和失败检查未执行 |

“项目选择了私有实现”不能直接证明 Charm 缺能力；“仓库存在同名组件”也不能证明能力已经可用。

## 边界

- 真实板项目可作为 EvidenceRig 暴露时序、cache、外设和装配问题；其 HAL 接线、shell 或 workaround
  不自动成为跨平台契约。
- pre-graph、fault 和极早期路径可直连 port/UART；正常 console/binding 建立后继续直连，必须记录理由
  和退出条件。
- 共享协调层接管重 runtime 前，优先消费无副作用、可重复读取的 status/snapshot。可打印不等于已经
  形成 Capability Contract。
- Host、QEMU 和真实板是不同证据域；metadata、schema、README 或 build-only 不能替代真实板运行。

## 审计证据

每个结论至少记录 consumer 行为、source/CMake/profile 默认接线、私有路径成因、可复现问题、修复后的
编译检查和行为证据。

## 修复规则

1. 先确认 consumer、行为和失败语义，再选择实现。
2. 先迁移使用路径，再删除旧接口；保留例外时写明范围和退出条件。
3. 默认路径进入 profile、binding、template 或 build check；文档推荐不算完成。
4. 用 source/CMake 检查约束依赖，不用全局 locator、平台宏或 forward declaration 绕过边界。
5. 分环境保留编译检查、成功行为和关键失败/缺失证据。

修复记录应包含 consumer、目标 contract/implementation、旧路径退出条件、检查命令、正反例、
日志位置与剩余风险。整理实现不会自动授予新名称或接口 Core 身份。

## 与 Core 的关系

`DefaultConsolePath`、`EarlyConsole`、`ServiceSnapshotContract` 与 `EvidenceRig` 是局部审计术语，不属于
Constitution 已批准的 Core vocabulary。

旧 H747 路径、完整问题清单和 P0-P4 排期见
[`../archive/architecture-inventory-v0/real_board_landing_gap_audit_v0.md`](../archive/architecture-inventory-v0/real_board_landing_gap_audit_v0.md)。
