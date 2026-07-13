# I2C Device Evidence v0 归档摘要

> `status`: `archived`

本文保留早期 I2C fact/evidence 编排的证据边界。现行接口行为见
[`i2c_device_contract_v0.md`](../../architecture/i2c_device_contract_v0.md)。

## 历史证据结论

早期链路覆盖 board/package fact、Host fact composition、mock WHOAMI transaction 和 Host bring-up
fixture，并验证相同 graph 下 evidence sidecar 的 `missing -> satisfied` 投影。

这些都属于声明、Host composition、mock transaction 或 schema/tooling 证据。即使 fixture 输出
`i2c.probe.board_real`，名称和 producer label 也不能把 Host 输入升级为真实硬件证据，更不能证明 I2C
controller、pinmux、clock、power 或 bus timing。

历史 case、sample、脚本名称和输出目录只用于追溯当时工具链，不构成当前 regression inventory；需要
复现时从 Git 历史和现有 producer/validator 重新确认。

## Real-board 最低证据

真实板 probe 至少记录：

- board 身份、firmware revision、producer 和执行时间；
- controller、pinmux、clock 与 power 的实际来源；
- slave address、register、expected identity 与 transaction 参数；
- 成功路径和至少一个 bus/device failure；
- 原始板级日志或可追溯 artifact；
- 与 no-hardware baseline 的差异；
- 明确的 Host、QEMU、准真实或 real hardware evidence domain。

在这些证据存在前，不恢复 `candidate/admitted` 语言，也不把历史 fact label 提升为公共 contract
vocabulary。
