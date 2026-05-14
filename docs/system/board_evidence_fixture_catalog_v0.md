# Board Evidence Fixture Catalog v0

本文是 `bringup evidence pipeline v0` 的执行台账。

它不定义新的 schema，不新增 C++ API，也不把任何 Host fixture 升级为真实硬件 evidence。

它只回答一个更窄的问题：

> **当前哪些 board / probe evidence 输入形态已经可以被导出、比较、校验和复验。**

上位语义入口：

- [`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md)
- [`artifact_report_v0.md`](artifact_report_v0.md)

设备契约入口：

- [`../architecture/i2c_device_contract_v0.md`](../architecture/i2c_device_contract_v0.md)
- [`../architecture/device_contract_evidence_ladder_v0.md`](../architecture/device_contract_evidence_ladder_v0.md)

## 1. 当前定位

当前 board evidence v0 只收束两类事实：

- board/package 已知事实
- board/probe evidence 输入形态

它的目标不是证明真实板子已经通过 probe，而是让这些事实可以进入：

```text
export_bundle -> artifact_report -> inspector -> compare / validation
```

这让 Charm 能先稳定回答：

- 这条证据来自哪个 producer
- 它声明了哪些 facts
- 它要求哪些 facts
- 哪些 required facts 已满足
- 哪些 required facts 仍缺失
- 同一条 graph 未变时，evidence sidecar 是否发生漂移

## 2. 当前 fixture catalog

| Fixture / case | 类型 | Producer | 作用 | 当前边界 |
| --- | --- | --- | --- | --- |
| `board-package-facts-smoke` | `fact_only` | board package facts | 导出 board/package 已知事实 | 只说明声明事实，不说明 probe 成功 |
| `board-i2c-fact-composition-smoke` | `fact_only` | board facts + I2C contract facts | 验证多来源 facts 可以组合进入 artifact report | 仍是 composition evidence，不是硬件 probe |
| `i2c-whoami-probe-evidence-smoke` | `fact_only` | no-hardware WHOAMI probe | 证明 `driver.i2c_whoami_probe` 可导出 no-hardware probe evidence | `i2c.probe.board_real` 保持 `missing` |
| `board-i2c-whoami-bringup-evidence-smoke` | `fact_only` | Host fixture `board.bringup` | 把 `i2c.probe.board_real` 作为已提供 fact 输入 artifact report | 使用 mock transaction，不等价真实硬件 |

当前第一条完整 board/probe 样板是：

- `board-i2c-whoami-bringup-evidence-smoke`

它复用 `driver.i2c_whoami_probe` 与 mock I2C transaction 跑通 WHOAMI 成功路径，
但导出的 `fact_evidence` 明确把 provider source 标成 `board.bringup`。

这一步的价值是把早先 synthetic compare 里的 provider 形态，
收成一个正式可导出的 Host fixture 输入。

## 3. 当前 sidecar 样例

当前稳定样例包括：

- `schemas/examples/system_compiler.fact_evidence.v0.board_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_composition.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.i2c_whoami_probe.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_whoami_bringup.sample.json`

其中 `board_i2c_whoami_bringup` 样例当前钉住：

- `declared_facts`
  包含 `i2c.device:i2c1@0x18`、`i2c.evidence:whoami_probe`、`i2c.probe.board_real`
- `required_facts`
  继续沿用 WHOAMI probe 所需 facts
- `audit_provided_facts`
  包含 `i2c.probe.board_real`
- `raw_facts`
  将 `i2c.probe.board_real` 的 `source` 标为 `board.bringup`

`i2c.probe.board_real` 当前仍是 probe-local evidence fact name，
不是 admitted I2C contract vocabulary。

## 4. 当前复验入口

如果只想复验整条 I2C board evidence v0 链路，优先使用：

```powershell
./scripts/materialized_graph_i2c_board_evidence_chain_smoke.ps1 -OutputRoot cmake-build-i2c-board-evidence-chain-smoke
```

这条 chain smoke 当前串行验证：

- `i2c-whoami-probe-evidence-smoke`
- `board-i2c-whoami-bringup-evidence-smoke`
- `materialized_graph_i2c_whoami_board_bringup_evidence_compare_smoke.ps1`
- `schemas/examples/system_compiler.fact_evidence.v0.board_i2c_whoami_bringup.sample.json`

它的 summary 默认写到：

```text
cmake-build-i2c-board-evidence-chain-smoke/i2c_board_evidence_chain_smoke.summary.json
```

## 5. 分步调试入口

如果需要拆开定位，可以按下面顺序跑。

先导出 no-hardware WHOAMI probe evidence：

```powershell
./scripts/export_materialized_graph.ps1 -Case i2c-whoami-probe-evidence-smoke -OutputRoot out/i2c-whoami-probe-bundle
```

再导出 Host fixture board evidence：

```powershell
./scripts/export_materialized_graph.ps1 -Case board-i2c-whoami-bringup-evidence-smoke -OutputRoot out/board-i2c-whoami-bringup-bundle
```

再生成对应 artifact report：

```powershell
./scripts/export_system_compiler_artifact_report.ps1 -BundleRoot out/board-i2c-whoami-bringup-bundle -Case board-i2c-whoami-bringup-evidence-smoke -OutputRoot out/board-i2c-whoami-bringup-artifact-report
```

再校验 bundle 与 report：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py --bundle-root ./out/board-i2c-whoami-bringup-bundle ./out/board-i2c-whoami-bringup-artifact-report/board-i2c-whoami-bringup-evidence-smoke.artifact_report.json
```

如果要确认 producer-side evidence swap：

```powershell
./scripts/materialized_graph_i2c_whoami_board_bringup_evidence_compare_smoke.ps1 -OutputRoot out/i2c-board-evidence-producer-compare
```

期望核心结论是：

```text
i2c.probe.board_real: missing -> satisfied
provider source: board.bringup
```

## 6. 与真实硬件 evidence 的边界

当前 Host fixture evidence 只证明：

- board/probe evidence 可以作为 `fact_evidence` sidecar 输入
- artifact report 可以消费该输入
- inspector / compare 可以解释 required fact 从 `missing` 到 `satisfied`
- chain smoke 可以一键复验这条输入形态

它不证明：

- 真实 I2C controller 已在板上工作
- 真实 pinmux / clock / power domain 已由硬件验证
- 真实 sensor / codec / PMIC 已经 probe 成功
- I2C contract 已经满足 `candidate`

因此 I2C 仍保持 `experimental`。

真实硬件 evidence 接入前，不能把 `board-i2c-whoami-bringup-evidence-smoke`
当作 candidate evidence 使用。

## 7. 下一张卡应补什么

下一条真正有价值的 board evidence 不应继续复制 Host fixture。

更值当的方向是补一条真实或准真实 probe：

- 真实芯片 driver，例如 sensor / EEPROM / codec / PMIC
- 真实板级 probe log 或 board bringup evidence
- 明确的 controller / pinmux / clock / power domain facts
- 能被 artifact report 消费的 `fact_evidence` sidecar
- 能与 no-hardware baseline 做 compare 的 producer-side smoke

接入真实或准真实 I2C board/probe evidence 前，先按
[`i2c_board_probe_evidence_readiness_checklist_v0.md`](i2c_board_probe_evidence_readiness_checklist_v0.md)
检查 producer、facts、sidecar 与 compare baseline。

在这之前，本目录中的 board evidence fixture 只承担“输入形态样板”和“工具链回归”职责。
