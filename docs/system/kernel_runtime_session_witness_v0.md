# Kernel Runtime Session Witness v0

这份文档收口下一阶段的最小目标：

> 把 `minimal_kernel_runtime` 的一次运行会话建模成 host 语义、ARMv7-A QEMU 机器执行、runtime evidence bundle、witness bundle 与 world compare 可以共同指向的第一类对象。

## 结论

`Kernel Runtime Session Witness v0` 的中心不是 QEMU 日志，也不是某份 report，而是 `kernel_runtime_session`。

它回答的问题是：

- 这次 session 属于哪个 world
- 它运行在哪个 leaf / board / profile 上
- upper-half host verifier 证明了哪些语义
- lower-half QEMU 证明了哪些机器入口
- runtime 里 tick / trap / thread / task syscall / handoff continuity 是否站住
- 失败时塌在 semantic、machine、runtime、bundle 还是 world contract 面
- 上层 witness bundle / world compare 应该消费哪个稳定对象，而不是直接追原始日志

## 当前阶段边界

上一阶段可以命名为：

```text
ARMv7-A Lower-Half Evidence Closure
```

它证明真实 ARMv7-A 机器状态下，异常、timer、trap、syscall、thread、handoff、live runtime 的 lower-half seam 已经形成可回归证据。

下一阶段命名为：

```text
Kernel Runtime Session Witness v0
```

它不以新增 OS 功能为主，而是把一次 minimal-kernel runtime 会话正式出口成可复盘、可比较、可诊断的对象。

## Session 是什么

`kernel_runtime_session` 是一次最小内核运行会话的证明对象。

它不是：

- 原始 QEMU 串口日志
- 某个 smoke 脚本的私有 summary
- witness bundle 的替代品
- world compare 的替代品
- 一个已经承诺完整用户态、进程、VFS 或 POSIX 的 OS 对象

它是：

- host semantic witness 与 QEMU machine witness 的共同投影
- runtime evidence bundle 可引用的 session summary
- witness bundle 未来可以挂载的 witness entry
- world compare 未来判断 session drift / collapse 的稳定输入

推荐管线是：

```text
host verifier
  -> semantic session facts

ARMv7-A QEMU lower-half smoke
  -> machine session facts

runtime evidence bundle
  -> kernel_runtime_session.summary.json

system compiler witness bundle
  -> session witness entry

world compare
  -> standing / improved / drifted / collapsed
```

重要约束：

```text
session object 定义需要哪些事实
runtime ledger 记录事实发生顺序
QEMU log parser 只是事实采集器之一
```

不能反过来让串口日志里刚好出现了什么，决定 session 对象长什么样。

## 语义分层

### Semantic Witness

`semantic_witness` 由 host 侧 verifier 证明。

它证明：

- runtime glue / bridge / loop 的上半层语义仍然闭合
- trap / syscall / task message / session API 的 host 契约仍然自洽
- cold start 与 warm reuse 的 host 证据仍然可复验

它不证明：

- 真实 ARMv7-A 异常入口
- 真实 trap frame capture / writeback
- 真实 timer IRQ 或 GIC 路径

### Machine Witness

`machine_witness` 由 ARMv7-A QEMU lower-half smoke 证明。

它证明：

- exception / interrupt / timer / trap / context / runtime loop 入口在机器世界里可观察
- live runtime、task syscall、handoff landing 等 lower-half seam 在 QEMU 上仍然站住
- lower-half 证据可以被收束到同一个 canonical world

它不证明：

- host stub 的完整语义覆盖
- 真机 DDR / SRAM / BootROM / board-specific clock tree 约束
- 完整用户态隔离或进程模型

### Runtime Facts

`runtime` 是 semantic 与 machine 共同指向的会话事实。

v0 先关注：

- `tick`
- `trap`
- `thread`
- `task_syscall`
- `handoff_continuity`

其中 `handoff_continuity` 当前只属于 session continuity，不单独膨胀成新 world。

只有当 handoff 开始承诺 image format、payload verification、slot / rollback、multi-stage boot chain、storage-backed launch 或真实板级 boot medium 时，才值得拆成独立 world。

## Ledger 分层

v0 保持三层：

```text
phase ledger
  证明阶段顺序：
  boot -> mmu -> exception -> timer -> trap -> runtime -> handoff

runtime ledger
  证明运行会话事件：
  tick observed
  trap decoded
  syscall dispatched
  task resumed
  thread switched
  handoff continuity preserved

session summary
  证明这次运行会话整体成立
```

可以这样理解：

```text
phase ledger 是骨架
runtime ledger 是血流
session summary 是体检报告
```

当前第一刀只新增 `runtime_ledger.json` 与 `kernel_runtime_session.summary.json` 的最小出口，不要求立刻重构现有 QEMU log parser。

## Failure Taxonomy v0

session witness 的失败项需要能直接喂给后续 witness bundle / world compare。

v0 先冻结这些 failure code：

```text
decode_failed
unsupported_service
unbound_adapter
unbound_bridge
writeback_failed
missing_phase
unexpected_phase
timeout
spurious_interrupt
trap_not_observed
tick_not_observed
thread_not_resumed
handoff_not_landed
handoff_continuity_broken
host_semantic_mismatch
machine_witness_missing
world_contract_missing
```

failure entry 至少包含：

```json
{
  "code": "handoff_continuity_broken",
  "domain": "machine",
  "layer": "lower_half",
  "focus": ["handoff", "session"],
  "required": true,
  "phase": "handoff.live",
  "message": "handoff launch occurred but landing-side runtime package was not re-consumed"
}
```

这样 world compare 看到的不是“某个脚本失败了”，而是：

```text
minimal_kernel_runtime world 的 session continuity witness 崩了
```

## Artifact v0

第一版 session exporter 输出：

```text
session/
  kernel_runtime_session.summary.json
  runtime_ledger.json
  report.md
  check.txt
```

对应 schema：

- `schemas/minimal_kernel.kernel_runtime_session.v0.schema.json`

对应最小 sample：

- `schemas/examples/minimal_kernel.kernel_runtime_session.v0.sample.json`

对应独立入口：

- `scripts/export_minimal_kernel_runtime_session.py`
- `scripts/minimal_kernel_runtime_session_smoke.ps1`
- `scripts/minimal_kernel_runtime_session_witness_smoke.ps1`
- `scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1`

聚合入口 `minimal_kernel_runtime_session_witness_smoke.ps1` 不再只是 console smoke。
它会在输出根目录额外导出：

```text
summary.json
report.md
check.txt
```

其中根 `summary.json` 会引用 session exporter、world compare session drift、witness exporter failure export 三条子证据链，并摘取 session status、runtime facts、session drift failure codes 等最小结论。
这让 `kernel_runtime_session` 的阶段性闭环可以被 CI、front page、evidence shelf 或后续 witness bundle 消费，而不是只停留在终端输出里。

根 `summary.json` 对应的机器契约是：

- `schemas/minimal_kernel.runtime_session_witness_smoke.v0.schema.json`
- `schemas/examples/minimal_kernel.runtime_session_witness_smoke.v0.sample.json`
- `scripts/validate_minimal_kernel_runtime_session_witness_smoke.py`
- `scripts/check_minimal_kernel_runtime_session_witness_smoke_summary.ps1`
- `scripts/ci_minimal_kernel_runtime_session_witness_smoke.ps1`

其中 validator 负责 schema 与引用路径，check 脚本负责断言 session standing、两条 session drift 投影、failure code 与 missing runtime fact。
CI / 人工验收优先调用 `ci_minimal_kernel_runtime_session_witness_smoke.ps1`，它默认把产物落到 `out/minimal-kernel-runtime-session-witness-smoke`，并在根 smoke 之后再次执行 validator 与 gate。
如果额外给出 `-InspectCompareSummaryOutputRoot`，同一条 CI 入口还会顺带执行 `inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1`，把 inspect compare 对象也纳入持续守护。

runtime evidence bundle 会把它作为 `summary.json.session` 侧车回填。
system compiler witness bundle 也可以通过 `kernel_runtime_session` witness kind 正式消费它。

如果只是想只读消费这条聚合根，而不重跑 smoke，可以直接运行：

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json -ShowArtifacts
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json -BaselineSummary baseline/minimal-kernel-runtime-session-witness/summary.json
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json -BaselineSummary baseline/minimal-kernel-runtime-session-witness/summary.json -CompareSummaryPath out/minimal-kernel-runtime-session-witness-compare/summary.json
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke.ps1 -Summary out/minimal-kernel-runtime-session-witness-smoke/summary.json -ShowNarratives -AsJson
```

这条 inspect 入口只读取根 `summary.json`，回答：

- 当前 `kernel_runtime_session` 是否 standing
- synthetic drift 与 witness-export drift 各自塌在哪个 domain / focus
- 哪个 failure code 与 missing runtime fact 导致了 session continuity collapse
- 上层如果要继续追 report / check / runtime ledger，应该沿着哪些 artifact path 下钻
- 如果给出 `-BaselineSummary`，当前这份 witness summary 相对上一份在哪些 result / runtime facts / failure taxonomy 上发生了漂移
- 如果同时给出 `-CompareSummaryPath`，上述漂移会被导出成机器可读的 `minimal_kernel.runtime_session_witness_inspect_compare/v0` 对象

对应 compare smoke：

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1
./scripts/inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1
```

## World Compare Projection

`world compare` 不直接读取原始 QEMU log，也不绕过 witness bundle 去解析散工件。

当 `kernel_runtime_session` witness entry 发生 regression 时，compare 会从 entry observations 投影 `collapse_surface.session_drift`：

- `regressed_sessions`
- `required_regressed_sessions`
- `affected_domains`
- `affected_focus`
- `missing_runtime_facts`
- `failure_codes`

这层投影使用 witness bundle 已经导出的 session observations，例如：

```text
session_status=collapsed
semantic=standing
machine=standing
runtime=tick:True trap:True thread:True task_syscall:True handoff:False
failure=handoff_continuity_broken domain=runtime layer=lower_half phase=handoff.live focus=handoff,continuity,session
```

这样 world compare 看到的不是“某个 witness fail 了”这一句粗粒度结论，而是可以继续说明：

```text
minimal_kernel_runtime world 的 session continuity witness 发生 runtime-domain collapse。
```

对应定向 smoke：

```powershell
./scripts/minimal_kernel_runtime_session_witness_smoke.ps1
```

它会串起 session exporter、world compare session drift、witness exporter failure export 三条低成本回归。

如果只想单独验证 world compare 对 session witness regression 的投影，可以运行：

```powershell
./scripts/system_compiler_world_compare_session_drift_smoke.ps1
```

如果要验证 `kernel_runtime_session.summary.json` 的 failure entry 能经由 witness bundle exporter 进入 world compare，可以运行 exporter 级闭环 smoke：

```powershell
./scripts/system_compiler_witness_session_failure_export_smoke.ps1
```

CI / gate 可以通过 `scripts/check_system_compiler_world_compare_summary.ps1` 直接断言 session drift 面：

```powershell
./scripts/check_system_compiler_world_compare_summary.ps1 `
  -Summary out/system-compiler-world-compare/summary.json `
  -RequireSessionDrift true `
  -RequireSessionDomain session,runtime `
  -RequireSessionFocus session,runtime,handoff,continuity `
  -RequireSessionFailureCode handoff_continuity_broken `
  -RequireMissingRuntimeFact handoff
```

## 当前非目标

这一刀不做：

- 完整 OS feature buffet
- 用户态隔离
- 进程 / ELF loader / VFS / shell
- RK3506 真机主线
- 大规模 C++ lower-half 重构
- world compare 直接读取原始 QEMU log

当前目标只有一个：

> 让 `session` 先成为 Charm 最小内核证据世界里的正式对象。
