# Canonical Worlds

这个目录收纳“世界级”样本，而不是单点 verifier。

它们的职责不是替代现有 `runtime_*_host` 或 QEMU smoke，
而是把一组 case、契约和 witness 收成同一个可复盘对象：

- 这个世界试图证明什么
- 它依赖哪些 contract
- 它由哪些 witness 共同作证
- 如果 compare 漂移，最先应该怀疑哪条证据线

当前第一批 world manifest 偏向最小内核 runtime / syscall / trap 这条主线。
