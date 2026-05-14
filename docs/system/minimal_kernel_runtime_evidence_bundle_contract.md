# Minimal Kernel Runtime Evidence Bundle Contract

## 目的

这份文档是当前最小内核运行时证据总入口。

它解决的不是单个 smoke 怎么跑，而是下面这个更大的问题：

- 上半层 host verifier 证明了什么
- 下半层 ARMv7-A QEMU leaf 又证明了什么
- 两者怎样在同一份证据包里对齐，而不是各自零散存在

## 入口分层

- `scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1`
  - 上半层证据入口
  - 同时产出 cold start 与 warm reuse 两套 host 证据
- `scripts/minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1`
  - 下半层证据入口
  - 聚焦 `runtime-trap / runtime-live / task-syscall / handoff-live` 等 ARMv7-A QEMU lower-half smoke
- `scripts/minimal_kernel_runtime_evidence_bundle.ps1`
  - 总证据入口
  - 把 host dual bundle、qemu bundle、kernel runtime session summary 与 system compiler witness bundle 收进同一个 artifact 根目录

## 证据边界

### Upper-Half Host

host dual bundle 主要证明：

- 上半层 `runtime_*_host` verifier 仍然可运行
- 冷启动 configure/build/run 路径仍然成立
- 同一环境下 warm reuse 的 configure 跳过仍然成立
- warm 报告可以相对 cold baseline 给出直接 improvement / regression 视角

它不证明：

- 真实 ARMv7-A 异常入口
- 真实 lower-half frame capture / writeback
- 真机或 QEMU 中断/时钟/上下文切换路径

### Lower-Half ARMv7-A QEMU

qemu bundle 主要证明：

- `arch-ingress-seam`
- `runtime-trap`
- `runtime-live`
- `task-syscall`
- `handoff-live`

这些 lower-half smoke 在同一个 `debug` 构建上仍然闭环成立，并且会留下每个 case 的独立日志工件。
总证据入口不再硬编码早期 case 数，而是消费 qemu bundle 自身导出的 `case_count / completed_case_count`，避免 session 证词在 lower-half case 缺席时误判为站立。

它不证明：

- 上半层 verifier 的命名与契约是否仍然一致
- host stub 语义是否仍然完整
- reuse configure 这类本地开发效率证据

## Artifact 结构

默认总 bundle 结构如下：

```text
out/minimal-kernel-runtime-evidence/
  host/
    ci/
    daily/
  qemu/
    cases/
  session/
    kernel_runtime_session.summary.json
    runtime_ledger.json
    report.md
    check.txt
  witness/
  summary.json
  report.md
  check.txt
  host.bundle.log
  qemu.bundle.log
  witness.bundle.log
```

其中：

- `host/ci` 对应 cold start host 证据
- `host/daily` 对应 warm reuse host 证据
- `qemu/cases/*` 保留 lower-half case 日志
- `session/*` 收口 `kernel_runtime_session` 对象、runtime ledger 与 session check
- `witness/*` 收口 canonical world 对应的 witness summary / report / check
- 根 `summary.json / report.md / check.txt` 是这次总证据包的聚合视图
- 根 `summary.json` 现在也会直接回填 `report_markdown_path / check_text_path / session / witness_bundle`，方便上层自动化只消费一个入口

## Kernel Runtime Session

`session/kernel_runtime_session.summary.json` 是当前 bundle 的共同被证明对象。

它把 host 侧 semantic witness、ARMv7-A QEMU 侧 machine witness、runtime facts、
runtime ledger 引用和 session verdict 收成一个对象。它不替代 host 或 QEMU 原始证据，
而是先作为 runtime evidence bundle 的正式 `session` 侧车 artifact，
再由 system compiler witness bundle 提升为 `kernel_runtime_session` witness entry。

其中 `arch_ingress_seam` 是 session machine witness 当前的首选 lower-half ingress anchor：
它把 exception / interrupt / timer / trap / context / runtime-loop 入口作为同一条 QEMU seam 投影进 session。
旧 QEMU summary 没有这条 case 时，session exporter 仍可用既有 runtime/trap/thread/task/handoff case 做兼容推断，但新的总证据包应让 `machine_witness.standing_cases` 显式包含 `arch_ingress_seam`，并在 `runtime_ledger.json` 中出现 `arch.ingress.seam` 事件。

对应契约入口：

- `docs/system/kernel_runtime_session_witness_v0.md`
- `docs/system/minimal_kernel_runtime_ledger_fact_contract_v0.md`
- `schemas/minimal_kernel.kernel_runtime_session.v0.schema.json`

## 机器可读契约

当前总证据 summary 已补齐独立 schema：

- `schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json`
- `schemas/minimal_kernel.kernel_runtime_session.v0.schema.json`

本地或 CI 如需校验 summary 结构与引用工件完整性，使用：

```powershell
python ./scripts/validate_minimal_kernel_runtime_evidence.py `
  --bundle-root out/minimal-kernel-runtime-evidence
```

这个校验器会做两件事：

- 用 schema 校验根 `summary.json` 的结构
- 检查 summary 中引用到的 host / qemu / session / witness / report / check / case log 工件是否都存在
- 复核 `session`、`kernel_runtime_session.summary.json` 与 `runtime_ledger.json` 的基础一致性

`runtime_ledger.json` 的事实语言、phase 顺序、status/domain vocabulary 与 `ledger.event_count == runtime_ledger.events.length` 关系由 `docs/system/minimal_kernel_runtime_ledger_fact_contract_v0.md` 约束。该 ledger 只记录 exporter 已消费的 summary facts，不回读 raw host/QEMU/session logs。

如果只想验证 session 对象的第一版出口，可以先跑旁路 smoke：

```powershell
./scripts/minimal_kernel_runtime_session_smoke.ps1
```

它默认消费 `schemas/examples/minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json`，
并输出：

```text
cmake-build-minimal-kernel-runtime-session-smoke/
  kernel_runtime_session.summary.json
  runtime_ledger.json
  report.md
  check.txt
```

这条旁路入口先证明 `session` 对象本身站得住；总 runtime evidence bundle 已经会把 `session` 作为侧车视图回填到根 `summary.json`。
system compiler witness bundle 会把它提升为独立 `kernel_runtime_session` witness entry。

如果要验证 `session` 作为 witness 聚合对象的完整闭环，优先跑：

```powershell
./scripts/ci_minimal_kernel_runtime_session_witness_smoke.ps1 `
  -Clean `
  -InspectCompareSummaryOutputRoot out/minimal-kernel-runtime-session-witness-inspect-compare
```

这样可以同时守住两层对象：

- 根 `minimal_kernel.runtime_session_witness_smoke/v0` summary
- `minimal_kernel.runtime_session_witness_inspect_compare/v0` compare 对象

它默认输出：

```text
out/minimal-kernel-runtime-session-witness-smoke/
  summary.json
  report.md
  check.txt
  session/
  world_compare_session_drift/
  witness_session_failure_export/
```

这条入口会同时证明：

- standing `kernel_runtime_session` 可以导出为根证据对象
- synthetic session drift 可以被 world compare 投影
- collapsed session 可以经由 witness exporter 进入 world compare
- 根 `summary.json` 可以通过 schema validator 与语义 gate

如果只是想消费这条聚合根、确认 session standing / drift / failure taxonomy，而不想重新执行 smoke，可以直接运行：

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 `
  -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json `
  -ShowArtifacts
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 `
  -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json `
  -BaselineSummary baseline/minimal-kernel-runtime-session-witness/summary.json
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 `
  -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json `
  -BaselineSummary baseline/minimal-kernel-runtime-session-witness/summary.json `
  -CompareSummaryPath out/minimal-kernel-runtime-session-witness-compare/summary.json
```

它会把根 `summary.json` 里的 session 状态、两条 drift 投影、missing runtime facts、failure codes 与关键 artifact path 收成稳定的只读视图；如果同时给出 `-BaselineSummary`，还会额外收口 result / runtime facts / failure taxonomy 的差分视图；如果再给 `-CompareSummaryPath`，这份差分会被落成正式 compare 对象，方便上层继续消费。

## 本地验证

如果要在本地复现当前总证据链，优先直接跑：

```powershell
./scripts/minimal_kernel_runtime_evidence_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-evidence `
  -HostExamples runtime_minimal_host `
  -HostJobs 8 `
  -QemuBuildJobs 8
```

期望信号：

- `host/ci/report.md` 显示 `Profile: ci`
- `host/daily/report.md` 显示 `Profile: daily`
- `host/daily/report.md` 带 `Comparison` 段
- `qemu/report.md` 显示 lower-half bundle 当前 smoke 集合全部站住
- `session/kernel_runtime_session.summary.json` 显示 `session_status: standing`
- `session/kernel_runtime_session.summary.json` 的 `machine_witness.standing_cases` 包含 `arch_ingress_seam`
- `session/runtime_ledger.json` 包含 `arch.ingress.seam`
- `witness/report.md` 显示 canonical world 与 witness entry 汇总
- 根 `report.md` 同时汇总上半层、下半层与 witness 证据

## CI 验收

仓库当前已提供统一的总证据 workflow：

- `.github/workflows/minimal-kernel-runtime-evidence.yml`

它的职责不是重新拼一套分散步骤，而是直接调用：

- `scripts/minimal_kernel_runtime_evidence_bundle.ps1`

当前 CI 形态约定如下：

- 在 `windows-latest` 上同时准备 host 侧 CLANG64 工具链、`arm-none-eabi` 裸机工具链、`qemu-system-arm`
- 统一把产物落到 `out/minimal-kernel-runtime-evidence`
- 把根 `report.md` 发布到 workflow step summary
- 上传整包 artifact，而不是只上传某个局部 smoke 结果

## 当前注意事项

- `task-syscall` lower-half smoke 当前默认需要比早期更长的等待窗口，相关入口默认超时已统一上调到 `30s`
- 如果 CI 失败，优先先看根 `report.md / check.txt`，再沿着 `host.bundle.log` 与 `qemu.bundle.log` 下钻
