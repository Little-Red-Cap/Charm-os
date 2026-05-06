# I2C Board Probe Evidence Readiness Checklist v0

本文是 I2C real board/probe evidence 接入前的准备清单。

它不是新的契约，不是新的 JSON schema，也不是 I2C `candidate` 升级申请。

它只回答一个问题：

> **一条真实或准真实 I2C 板级 probe evidence 在进入 `export_bundle -> artifact_report -> inspector -> compare` 前，应该先准备好什么。**

上位入口：

- [`bringup_evidence_pipeline_v0.md`](bringup_evidence_pipeline_v0.md)
- [`artifact_report_v0.md`](artifact_report_v0.md)
- [`board_evidence_fixture_catalog_v0.md`](board_evidence_fixture_catalog_v0.md)

设备契约入口：

- [`../architecture/i2c_device_contract_v0.md`](../architecture/i2c_device_contract_v0.md)
- [`../architecture/device_contract_evidence_ladder_v0.md`](../architecture/device_contract_evidence_ladder_v0.md)

## 1. 当前定位

当前仓库已经有三层 I2C probe evidence：

| 层级 | 当前样板 | 说明 |
| --- | --- | --- |
| no-hardware baseline | `i2c-whoami-probe-evidence-smoke` | 使用 mock I2C transaction，`i2c.probe.board_real` 保持 `missing` |
| Host fixture board evidence | `board-i2c-whoami-bringup-evidence-smoke` | 使用 mock transaction，但 provider source 为 `board.bringup` |
| real board/probe evidence | 待补 | 需要真实或准真实 board/probe 输入，不由本 checklist 直接实现 |

本 checklist 的目标是给第三层准备入口。

它不要求本轮接真实硬件，也不把 Host fixture 当作真实硬件 evidence。

## 2. Evidence Producer Checklist

真实或准真实 I2C board/probe producer 接入前，必须先能回答：

- `source`
  evidence 来自哪个 example、board bringup 工具、board log 转换器或硬件 probe 路径。
- `producer`
  producer 名称必须能区分真实硬件、Host fixture、no-hardware mock。
- `subject.board`
  必须明确目标 board 名称，不能继续写成 `mock_i2c`。
- `active_facets`
  至少应能表达 `platform`、`board_package`、`io`、`driver_contract`、`probe_evidence`、`bringup_evidence` 中实际参与的 facet。
- `host_fixture`
  如果仍使用 host/mock transaction，必须在 `summary.host_fixture = true` 或等价摘要里诚实标出。
- `real_hardware`
  如果声明为真实硬件 evidence，必须能追溯到真实板级运行记录或真实 probe 输出。

推荐 producer 命名形态：

```text
board.bringup.real_probe+driver.<chip_or_probe_name>
```

不推荐：

```text
board.bringup
```

原因是它无法区分 Host fixture、真实硬件、板级 log 转换或 synthetic evidence。

## 3. Fact Evidence Shape Checklist

真实 I2C board/probe evidence 继续复用现有 `system_compiler.fact_evidence/v0` 形状。

必须使用现有字段：

- `summary`
- `facts.declared_facts`
- `facts.required_facts`
- `facts.audit_provided_facts`
- `raw_facts`

不新增 schema 字段。

`raw_facts` 中每条关键事实至少应说明：

- `name`
- `kind`
- `source`
- `role`
- `required`
- `state`

当前可接受的 `state` 仍保持现有报告语义：

- `provided`
- `missing`
- `unknown`

如果某个事实还没有证据，不要为了凑齐 checklist 把它写成 `provided`。

## 4. Required Facts Checklist

真实 I2C probe evidence 的 `required_facts` 至少应显式考虑：

- I2C device fact
  例如 `i2c.device:i2c1@0x18`
- probe register fact
  例如 `i2c.register:0x0f`
- expected identity fact
  例如 `i2c.expected_id:0x33`
- backend fact
  例如真实 backend、HAL adapter backend 或仍为 mock backend
- probe evidence fact
  例如 `i2c.evidence:whoami_probe`
- board/probe evidence fact
  当前仍可使用 `i2c.probe.board_real`
- controller / clock / pinmux / power facts
  如果 producer 能证明，就写入 `audit_provided_facts`；如果不能证明，就保持 missing 或暂不声明。

`i2c.probe.board_real` 当前仍是 probe-local evidence fact name。

它不进入 admitted I2C contract vocabulary，也不能单独把 I2C 升级为 `candidate`。

## 5. Minimal Real Evidence Package

一条最小真实或准真实 evidence 包应包含：

- 一个真实或准真实 I2C chip/probe target
  优先选择 sensor ID、EEPROM、PMIC register 或 codec register。
- 一个只依赖 `io.device_i2c` 的 driver/probe 路径
  不直接依赖 HAL、BoardCaps、mock 内部类型或 platform header。
- 一份可被 artifact report 消费的 `fact_evidence` sidecar
  至少能进入 `fact_resolution.required_fact_resolution`。
- 一条 no-hardware baseline
  用于对比 `i2c.probe.board_real` 或真实 backend/probe fact 的变化。
- 一条 producer-side compare 路径
  目标是解释 evidence producer 漂移，而不是伪造 graph 变化。

如果缺少真实板级运行记录，该 evidence 只能标为 Host fixture 或准真实 probe evidence。

## 6. Compare Baseline Checklist

真实 evidence 进入 compare 前，应至少准备一个 baseline。

推荐 baseline：

```text
i2c-whoami-probe-evidence-smoke
```

推荐 candidate：

```text
<real-board-i2c-probe-evidence-case>
```

compare 结论至少应能说明：

- graph 是否保持 `unchanged`
- `subject.board` 是否从 mock/fixture 漂移到真实 board
- `active_facets` 是否新增 `bringup_evidence`
- `i2c.probe.board_real` 是否从 `missing` 变为 `satisfied`
- provider source 是否从 no-hardware/mock/Host fixture 漂移到真实 board/probe producer

如果 graph 未变但 evidence sidecar 发生变化，结论应收束在 fact resolution / evidence drift，
不要把它误写成结构漂移。

## 7. Acceptance Checklist

接入真实或准真实 I2C board/probe evidence 前，至少应满足：

- 文档明确它是 no-hardware、Host fixture、准真实，还是真实硬件 evidence。
- `fact_evidence` sidecar 通过 `validate_materialized_graph_artifacts.py` 校验。
- artifact report 能显示对应 `artifacts.fact_evidence`。
- inspector 的 resource summary 或 fact resolution 能解释 required fact 状态。
- compare report 能解释 baseline 到 candidate 的 evidence drift。
- I2C contract 文档仍声明 `experimental`，除非真实硬件 evidence、真实 driver、正式 board bringup pipeline 都已补齐。

## 8. 当前非目标

本 checklist 当前不做：

- 不新增 C++ API
- 不新增 export manifest case
- 不修改 `system_compiler.fact_evidence/v0` schema
- 不修改现有 smoke
- 不要求 QEMU 或真实板运行
- 不把 I2C 升级为 `candidate`
- 不把 `i2c.probe.board_real` 提升为 admitted I2C vocabulary

## 9. 当前推荐下一步

下一步真正接 evidence 时，优先选择一个寄存器协议简单、可先用 mock 验证、未来可接真实板的目标：

- sensor ID probe
- EEPROM register-like probe
- PMIC register probe
- codec register probe

实现顺序建议保持：

1. 先写只依赖 `io.device_i2c` 的 driver/probe。
2. 先用 no-hardware mock smoke 验证成功路径与失败路径。
3. 再生成 `fact_evidence` sidecar。
4. 再接 artifact report 与 inspector。
5. 最后才进入真实 board/probe evidence compare。
