# Device Contract Admission v0 归档摘要

> `status`: `archived`

本目录记录 Constitution 建立前的设备接口准入实验。旧 interface policy、narrow waist、admission
matrix、evidence ladder 和 promotion queue 已退出默认阅读路径。

现行 implementation interface review：

- [`../../architecture/interface_admission_policy.md`](../../architecture/interface_admission_policy.md)

## 当时的本地标签

旧流程使用：

```text
proposed -> experimental -> candidate -> admitted -> deprecated
```

这些词用于记录 device contract prototype 的证据进度，不是 Core 裁决。快照中：

- I2C 被标为 `experimental`；
- SPI、GPIO、Block、Stream IO、Timebase 被标为 `proposed`；
- 没有接口因该台账自动获得 `Stable Boundary` 或 Core 身份。

## 保留的设计价值

- driver-facing 接口必须说明消费者、责任、执行、错误、资源和生命周期。
- mock、独立 backend、真实 driver、QEMU 和 real board 证明不同问题，不能互相替代。
- vendor SDK、board handle 和 runtime discovery 内部字段不应泄漏给可复用 driver。
- SPI bus/device、GPIO input/output/edge 等责任拆分值得在具体契约中继续验证。
- 动态设备需要稳定 slot 或 manager，不能裸发短命指针。

## 移出的流程噪声

- maturity 等级与 Core 准入混名；
- 同一证据在 matrix、ladder、queue 和单项契约中反复列出；
- P0/P1/P2 排期被写进架构契约；
- 强制每个接口产生 system compiler/artifact/explain 投影；
- 用 backend 数量或文档数量近似公共契约成立；
- `bool`、heap、同步 API 等 blanket 禁令。

各接口的具体讨论仍保留在 `docs/architecture/*_device_contract_v0.md`。需要旧台账或排期细节时使用 Git 历史，不恢复第二套准入法律。
