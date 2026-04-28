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
  - 聚焦 `runtime-trap / runtime-live / task-syscall` 等 ARMv7-A QEMU lower-half smoke
- `scripts/minimal_kernel_runtime_evidence_bundle.ps1`
  - 总证据入口
  - 把 host dual bundle、qemu bundle 与 system compiler witness bundle 收进同一个 artifact 根目录

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

- `runtime-trap`
- `runtime-live`
- `task-syscall`

这些 lower-half smoke 在同一个 `debug` 构建上仍然闭环成立，并且会留下每个 case 的独立日志工件。

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
- `witness/*` 收口 canonical world 对应的 witness summary / report / check
- 根 `summary.json / report.md / check.txt` 是这次总证据包的聚合视图
- 根 `summary.json` 现在也会直接回填 `report_markdown_path / check_text_path / witness_bundle`，方便上层自动化只消费一个入口

## 机器可读契约

当前总证据 summary 已补齐独立 schema：

- `schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json`

本地或 CI 如需校验 summary 结构与引用工件完整性，使用：

```powershell
python ./scripts/validate_minimal_kernel_runtime_evidence.py `
  --bundle-root out/minimal-kernel-runtime-evidence
```

这个校验器会做两件事：

- 用 schema 校验根 `summary.json` 的结构
- 检查 summary 中引用到的 host / qemu / witness / report / check / case log 工件是否都存在

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
