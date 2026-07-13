# Minimal-Kernel Semantic Witness Ladder Smoke

## 文档状态

- `status`: `supporting`
- `scope`: minimal-kernel 上半层 Host semantic witness 局部回归
- `source`: [`semantic_witness_ladder_smoke.ps1`](../../scripts/semantic_witness_ladder_smoke.ps1)

该 runner 逐层验证 task-message syscall、runtime/session API、protocol/dispatch 与 roundtrip witness，
避免只用最终 roundtrip 成功倒推中间 carrier 仍成立。case、target、required token 和工具参数只由脚本
维护，本文不复制清单。

## 执行与磁盘边界

runner 对每个 case 在同一个显式 build root 中重新配置、构建和运行；切换 case 前清理该目录，结束后
默认删除。build root 只允许位于脚本认可的仓库或临时根下，`KeepBuildRoot` 只用于失败定位。

因此它不会同时保留多个 case build tree，但会破坏指定 build root 的既有内容。调用方必须传入专用目录，
不能指向仍需复用的工程 build tree；磁盘受限时也不能保留失败产物后忘记清理。

## 失败语义

缺少 example、configure/build 失败、可执行文件缺失、进程非零或 witness token 不匹配都会使 runner
失败。脚本不生成 schema、session summary 或 runtime fact；文本 token 只是局部 fixture assertion。

## 证据边界

通过只证明该次 Host fixture 的上半层语义，不证明 ARMv7-A/QEMU lower-half ingress、真实上下文切换、
多 session 并发、公平性、完整 RPC 或 real-board 行为。

Host/QEMU/session 聚合的默认入口是
[`minimal_kernel_runtime_evidence_bundle_contract.md`](minimal_kernel_runtime_evidence_bundle_contract.md)；
本 runner 不是总证据链的替代入口。
