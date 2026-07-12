# I2C Device Evidence v0 归档摘要

## 状态

本页归档早期 I2C device contract、board fixture catalog 和 real-board readiness checklist 中的 evidence 编排。现行源码契约见：

- [`../../architecture/i2c_device_contract_v0.md`](../../architecture/i2c_device_contract_v0.md)

## 已有 Host evidence cases

| Case | 实际来源 | 能证明什么 |
|---|---|---|
| `board-package-facts-smoke` | 声明数据 | board/package fact shape |
| `board-i2c-fact-composition-smoke` | Host composition | 多来源 facts 可合并 |
| `i2c-whoami-probe-evidence-smoke` | mock transaction | no-hardware WHOAMI sidecar |
| `board-i2c-whoami-bringup-evidence-smoke` | Host fixture + mock | `board.bringup` producer 形状 |

第四项虽然提供名为 `i2c.probe.board_real` 的 fact，仍不证明真实硬件。fact 名称和 producer label 不能替代运行来源。

一键历史回归入口：

```powershell
./scripts/materialized_graph_i2c_board_evidence_chain_smoke.ps1 `
  -OutputRoot cmake-build-i2c-board-evidence-chain-smoke
```

它串行覆盖 WHOAMI sidecar、Host fixture、compare 和 sample validation。

## Sidecar 样例

- `schemas/examples/system_compiler.fact_evidence.v0.board_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_composition.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.i2c_whoami_probe.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_whoami_bringup.sample.json`

这些样例验证 schema/tooling，不是当前板级状态数据库。

## 历史工具路径

- `scripts/export_materialized_graph.ps1`
- `scripts/export_system_compiler_artifact_report.ps1`
- `scripts/validate_materialized_graph_artifacts.py`
- `scripts/materialized_graph_i2c_whoami_probe_evidence_compare_smoke.ps1`
- `scripts/materialized_graph_i2c_whoami_board_bringup_evidence_compare_smoke.ps1`

这些工具曾验证同一 graph 下 evidence sidecar 的 `missing -> satisfied` 差异。它们不验证 I2C transaction 的机器行为。

## 真实板证据最低要求

若未来接入 real-board probe，记录至少应包含：

- 明确 board、固件版本和 producer；
- controller/pinmux/clock/power 的真实来源；
- 具体 address/register/expected identity；
- 成功与至少一个失败路径；
- 原始板级日志或可追溯输出；
- 与 no-hardware baseline 的差异；
- 明确标注 Host fixture、QEMU、准真实或 real hardware。

在这些证据存在前，不恢复 `candidate/admitted` 语言，也不把 `i2c.probe.board_real` 提升为公共 contract vocabulary。
