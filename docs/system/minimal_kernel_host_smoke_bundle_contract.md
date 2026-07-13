# Minimal Kernel Host Smoke Bundle Contract

## 文档状态

- `status`: `supporting`
- `scope`: minimal-kernel Host semantic smoke 的 cold/warm 证据
- `source`: [`minimal_kernel_runtime_host_smoke_dual_bundle.ps1`](../../scripts/minimal_kernel_runtime_host_smoke_dual_bundle.ps1)

本 bundle 只证明 Host semantic smoke 的干净配置与热复用，不证明 ARM exception、IRQ、timer、真实上下文
切换或 real-board 行为。Host/QEMU 总入口见
[`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)。

## Cold 与 Warm

- cold 必须从 configure 开始，并留下同一次 dual run 可复用的构建状态；
- warm 必须引用 cold summary 作为 baseline，并复用已有 configure/build 状态；
- 两者运行同一组选定 examples，差异必须进入机器 summary/comparison，不能只写日志。

cold 证明入口可从干净状态配置、构建和运行；warm 证明同一入口可复用。任一结果都不能外推到其它
toolchain、QEMU 或真实板。

## 产物与失败

`summary.json` 是机器事实源，log、inspect、report 和 check 都是其下钻或人读投影。只要 summary
已产生，后续投影应尽量继续生成，即使 smoke 或最终 gate 失败；保留失败工件用于诊断，不表示失败被接受。

自动化必须读取 summary/check，不从 Markdown report 或进程 stdout 反推 verdict。case、token、wrapper、
参数、timeout 和输出路径由 dual bundle 及其调用脚本维护。

## 构建与磁盘

完整入口可能创建多个临时 build/output 目录。磁盘受限时应缩小 examples 集合、使用调用方明确拥有的
输出根，并在取证后清理；不得把这些临时目录当作长期可复用工程 build tree。

上半层局部回归见
[`minimal_kernel_semantic_witness_ladder_smoke_contract.md`](minimal_kernel_semantic_witness_ladder_smoke_contract.md)。
