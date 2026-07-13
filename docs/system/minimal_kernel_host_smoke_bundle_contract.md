# Minimal Kernel Host Smoke Bundle Contract

> status: `supporting`
>
> 本文约束 minimal-kernel Host semantic smoke 的 cold/warm 证据，不证明 ARM 异常、IRQ、timer
> 或真实上下文切换。Host + QEMU 总入口见
> [`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)。

## 入口

| 层 | 实现 |
|---|---|
| example configure/build/run engine | [`minimal_kernel_runtime_host_smoke.ps1`](../../scripts/minimal_kernel_runtime_host_smoke.ps1) |
| summary/report/check bundle | [`minimal_kernel_runtime_host_smoke_bundle.ps1`](../../scripts/minimal_kernel_runtime_host_smoke_bundle.ps1) |
| cold/warm wrappers | `ci_*` / `daily_*` host smoke scripts |
| 推荐双态入口 | [`minimal_kernel_runtime_host_smoke_dual_bundle.ps1`](../../scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1) |
| CI | [`minimal-kernel-host-smoke.yml`](../../.github/workflows/minimal-kernel-host-smoke.yml) |

## Cold 与 Warm

- Cold profile 必须执行 configure，并保留构建目录供同一次 dual run 复用。
- Warm profile 必须使用 cold summary 作为 baseline，并复用已有 configure/build 状态。
- 两者运行同一组选定 examples；差异必须出现在 summary/comparison 中，不能只写日志。

Cold 证明干净入口仍可配置、构建和运行；warm 证明预热后的同一入口可复用。任一结果都不证明
其它工具链、QEMU 或真实板。

## 产物与失败

Bundle 以 `summary.json` 为机器事实源，并生成 log、inspect、report 和 check 投影。只要 summary
已经产生，后续 inspect/report/check 应继续执行，即使 smoke 或最终 gate 失败。保留失败工件用于
诊断，不表示失败被接受。

CI 应始终发布可用 report，并上传 cold/warm 输出目录。自动化判断应读取 summary/check，不从
Markdown 文本反推 verdict。

## 本地入口

```powershell
./scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-host-smoke `
  -Examples runtime_minimal_host `
  -Jobs 8
```

该命令会创建多个 `cmake-build-verify-*` 目录；磁盘受限时不要运行全量 examples，且应在取证后
按调用方策略清理输出。
