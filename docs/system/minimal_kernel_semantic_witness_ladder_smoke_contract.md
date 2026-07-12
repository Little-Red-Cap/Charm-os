# Minimal-Kernel Semantic Witness Ladder Smoke

> status: supporting
>
> 本文说明 [`../../scripts/semantic_witness_ladder_smoke.ps1`](../../scripts/semantic_witness_ladder_smoke.ps1)
> 的用途与证据边界。case、target 和必需 token 以脚本为准。

## 职责

脚本顺序配置、构建并运行 15 个 host example，检查：

- configure/build/process 均成功；
- 每个 demo 的主结果包含 `ok=1`；
- 对应 witness 行包含脚本声明的 standing/collapsed/route/handoff token。

覆盖层次为：

```text
task-message syscall + frame + client + pump
-> runtime service/api + syscall api
-> session api + endpoint + protocol + dispatch + acceptor
-> session service + service loop + roundtrip
```

这避免只用最终 roundtrip 成功倒推所有中间 carrier 仍成立。具体 regex 不复制到
文档，修改 witness 输出时必须同步脚本。

## 构建行为

- 默认 generator 为 Ninja、配置为 Release、并行度为 1；
- 所有 case 复用同一个 `BuildRoot`，每个 case 前清理；
- 未传 `-KeepBuildRoot` 时，结束后删除 build root；
- build root 只能位于仓库或 `D:/Temp/charm-codex`。

本仓库内运行时可显式复用统一目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/semantic_witness_ladder_smoke.ps1 `
  -BuildRoot cmake-build-agent -Clean
```

成功终止行：

```text
[SEMANTIC-WITNESS-LADDER-SMOKE] result=ok cases=15
```

`-KeepBuildRoot` 只用于失败定位；磁盘紧张时不要启用。

## 失败与边界

缺少 example、CMake configure/build 失败、可执行文件缺失、进程非零或 token 不匹配
都会使脚本失败。脚本不生成 schema/sidecar，也不修改 runtime facts。

通过只证明这 15 个 host fixture 与当前文本 witness 一致，不证明：

- QEMU/ARMv7-A lower-half ingress；
- 多 session 并发、公平性或完整 RPC；
- runtime evidence bundle 的 Host/QEMU/session 汇总；
- 真实板行为。

更宽的证据入口是
[`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)；
ladder smoke 只是其中上半层语义的本地回归辅助线。
