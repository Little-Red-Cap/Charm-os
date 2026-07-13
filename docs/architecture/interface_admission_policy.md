# Implementation Interface Review

## 文档状态

- `status`: `supporting`
- `scope`: driver、backend、middleware 与 platform 之间的共享实现接口
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](charm_core_contract.md)

`Interface` 的当前裁决是 `Implementation / Tool`。本文只判断接口是否值得跨实现复用，不建立
第二套 Core 准入或 maturity/promotion 流程。旧流程见
[`device-contract-admission-v0`](../archive/device-contract-admission-v0/README.md)。

## 何时审查

候选接口必须有真实 consumer，并需要跨 driver/backend/platform 复用。Vendor SDK wrapper、单板
handle、demo glue、project policy 和只有一个试验 consumer 的类型应留在所有权目录。

## 最小审查面

| 维度 | 必须明确 |
|---|---|
| consumer | 谁依赖该行为；为什么现有更小接口不够；是否泄露 provider identity |
| behavior | 输入、输出、不变量、失败后状态；bus/address/endpoint/buffer/transaction 由谁拥有 |
| execution | 同步或 `would_block`；ISR/task/thread/domain 规则；timeout、callback 和 reentrancy |
| error | 消费者必须区分的类别；backend 映射；retry、detached、policy failure 与 rebuild |
| resource | heap、DMA、IRQ、clock、power、cache、memory region；缺失时的稳定失败 |
| lifecycle | bind/enable/detach/unexport/destroy；已发 handle 的失效行为；兼容与废弃范围 |

函数列表、concept satisfaction 和注册成功都不能替代上述语义。`util::Result/Errc` 是常用实现，
不是所有接口的 Core 强制；`bool` 是否足够取决于消费者需要的失败区分。

动态设备的 detach 与 stable-slot 边界见 [`driver_model.md`](driver_model.md)。不要把某个 Channel、
HAL 或 scheduler 的局部纪律扩写成所有接口的全局规则。

## 证据

| 证据 | 能说明什么 |
|---|---|
| local fake/mock 与 negative fixture | 行为、错误、顺序和缺失分支可重复 |
| 第二个独立 backend | 暴露首个实现泄漏的平台偶然性 |
| 真实 consumer | 接口由消费需求驱动，而非 provider 便利 |
| Host/QEMU/real-board run | 分别证明对应 execution environment；不能互相替代 |

接口留在局部 implementation，直到行为、ownership、error 和至少一个真实 consumer 可审查。若要
申请 `Stable Boundary` 或 Core，必须另行通过 Constitution 六问；backend 数量、smoke 数量和文档
数量都不会自动晋级。

## Review Record

专题契约只需记录：scope/consumer、behavior/non-goal、ownership/lifecycle、execution/error/resource、
实现证据和未覆盖风险。排期放 tracking，运行结果放 evidence log；不维护跨文件 maturity matrix、
evidence ladder 或 promotion queue。

本规则不建立统一 Driver 基类、全局 Interface Registry、共同错误 taxonomy 或共同生命周期，也不
要求 system compiler/artifact/explain 参与接口成立。
