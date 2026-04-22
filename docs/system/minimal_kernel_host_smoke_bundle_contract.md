# Minimal Kernel Host Smoke Bundle Contract

## 目的

这份文档是当前最小内核 host smoke 证据链的干净入口。

如果你现在要看的不是 host 上半层本身，而是“host + ARMv7-A QEMU lower-half”合起来的总证据入口，改读：

- `docs/system/minimal_kernel_runtime_evidence_bundle_contract.md`

当你想快速回答下面这些问题时，先读这里：

- 哪个脚本是底座，哪个只是薄包装
- 冷启动证据和热复用证据是怎么分层的
- GitHub workflow 这一条链到底要证明什么
- 即使最终 gate 失败，哪些工件也必须保留下来

## 入口分层

- `scripts/minimal_kernel_runtime_host_smoke.ps1`
  - 原始 smoke 引擎
  - 面向选定的 `runtime_*_host` example 批量执行 `configure / build / run`
- `scripts/minimal_kernel_runtime_host_smoke_ci.ps1`
  - 冷启动 profile
  - 固定使用 `-Fresh`
  - 在 bundle 场景下可以额外保留 build 目录
- `scripts/minimal_kernel_runtime_host_smoke_daily.ps1`
  - 热复用 profile
  - 固定使用 `-KeepBuildDirs -SkipConfigureIfPresent`
- `scripts/minimal_kernel_runtime_host_smoke_bundle.ps1`
  - bundle 引擎
  - 统一产出 `summary.json`、`smoke.log`、`inspect.txt`、`inspect.json`、`report.md`、`check.txt`
- `scripts/ci_minimal_kernel_runtime_host_smoke_bundle.ps1`
  - 冷启动 bundle 的薄包装
- `scripts/daily_minimal_kernel_runtime_host_smoke_bundle.ps1`
  - 热复用 bundle 的薄包装
- `scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1`
  - 冷启动 + 热复用的双态入口
  - 先跑 `ci bundle`，再把 cold `summary.json` 作为 baseline 传给 `daily bundle`
- `.github/workflows/minimal-kernel-host-smoke.yml`
  - 直接复用 dual bundle，在同一 runner 上完成 cold + warm 证据链

## Bundle 契约

### Cold Bundle

- profile 固定为 `ci`
- `configure` 应当真实执行
- bundle 必须保留 `cmake-build-verify-*`，供同一 job 里的 warm run 直接复用
- 默认报告标题为 `Minimal Kernel Host Smoke CI Report`

### Warm Bundle

- profile 固定为 `daily`
- `configure` 应当命中复用，而不是重新执行
- 在 CI 中，warm bundle 应当接收 cold 的 `summary.json` 作为 `-BaselineSummary`
- 默认报告标题为 `Minimal Kernel Host Smoke Daily Report`

### 报告与保活

- `report.md` 必须在最终 summary gate 之前生成
- 即使 smoke 过程返回非零，只要已经产出 `summary.json`，bundle 仍应继续完成 inspect / report / check，避免证据丢失
- 当传入 `BaselineSummary` 时，Markdown 报告应包含 `Comparison` 段，展示 regressions 与 improvements

## Workflow 契约

GitHub workflow 在一次运行里要同时证明两种状态：

1. 冷启动证据
   - 干净 configure 路径仍然可用
2. 热复用证据
   - 同一批 example 在 runner 被预热后可以复用 configure 状态

workflow 应当把两份 Markdown 报告都写入 `GITHUB_STEP_SUMMARY`，并把两套输出目录一起上传为同一个 artifact。

## 本地验证

在一台机器上复现 CI 的 cold -> warm 证据链，优先直接跑：

```powershell
./scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1 `
  -OutputRoot out/minimal-kernel-runtime-host-smoke `
  -Examples runtime_minimal_host `
  -Jobs 8
```

期望信号：

- `out/minimal-kernel-runtime-host-smoke/ci/report.md` 显示 `Profile: ci`
- `out/minimal-kernel-runtime-host-smoke/daily/report.md` 显示 `Profile: daily`
- warm 报告显示 `configure: total=0ms, executed=0, reused=...`
- warm 报告包含相对 cold baseline 的 `Comparison` 段
