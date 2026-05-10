# SPI Device Transaction Mock Readiness Checklist v0

本文是 SPI proposed contract 进入 transaction mock 之前的准备清单。

它不是新的契约，不是新的 API，也不是 SPI `experimental` 升级申请。

它只回答一个问题：

> **一条 SPI transaction mock 在进入 `artifact_report` / `compare` / evidence 链路前，应该先准备好什么。**

上位入口：

- [`../architecture/spi_device_contract_v0.md`](../architecture/spi_device_contract_v0.md)
- [`../architecture/device_contract_promotion_queue_v0.md`](../architecture/device_contract_promotion_queue_v0.md)
- [`../architecture/device_contract_admission_matrix_v0.md`](../architecture/device_contract_admission_matrix_v0.md)

## 1. 当前定位

当前 SPI 仍是 `proposed`。

它已经有：

- controller-facing `hal_spi`
- `hal::SpiBinding`
- `block::SpiFlashBinding`

但这些都还不是 driver-facing SPI transaction mock evidence。

这份清单只负责把第一张票的准备项固定住，不负责直接实现 mock。

## 2. Producer / Source / Subject / Facets Checklist

SPI transaction mock producer 接入前，必须先能回答：

- `source`
  evidence 来自哪个 example、脚本、host fixture 或 mock backend。
- `producer`
  producer 名称必须能区分 mock、Host fixture、准真实或真实 board/probe。
- `subject`
  必须明确 SPI 目标对象。
  至少要说明是 `board`、`chip` 还是 `display / flash / sensor` 这类 device subject。
- `active_facets`
  至少应能表达 `platform`、`board_package`、`io`、`driver_contract`、`transaction_mock`、`evidence`、`compare` 中实际参与的 facet。
- `host_fixture`
  如果仍然使用 host/mock transaction，必须在摘要里诚实标出。
- `real_hardware`
  如果声明为真实硬件 evidence，必须能追溯到真实板级运行记录或真实 probe 输出。

推荐 producer 命名形态：

```text
spi.transaction_mock.<target_name>
```

如果后续要接 Host fixture 或真实板级 evidence，再另起更明确的 producer 名称。

## 3. SpiBus / SpiDevice 边界

这份清单默认 SPI 仍按两层对象拆分：

- `SpiBus`
- `SpiDevice`

`SpiBus` 表示对整条 SPI bus 的受管访问能力。

`SpiDevice` 表示一个带 endpoint 语义的事务性设备访问能力。

带片选的外设 driver 默认应该依赖 `SpiDevice`，而不是依赖 `SpiBus` 后自己手动处理 CS。

推荐理解：

```text
driver -> SpiDevice -> managed transaction -> SpiBus/backend
```

而不是：

```text
driver -> SpiBus + GPIO CS + ad-hoc flush
```

## 4. Transaction Mock Coverage Checklist

第一张 SPI transaction mock 票至少应覆盖：

- transaction begin / end
- write bytes
- read bytes
- transfer bytes
- chip select asserted / deasserted expectation
- flush expectation
- backend failure

这张票不需要一开始就覆盖所有 SPI 变体。

暂不覆盖：

- quad / dual / octal
- DMA
- memory mapped mode
- async
- timeout
- replay / managed time

mock 语义最重要的是说明：

- driver 不手动管理 CS
- transaction 返回前 device / bus 处于定义状态
- backend failure 会消耗或终止 script

## 5. Error Semantics Checklist

SPI 公共错误语言尚未冻结。

在投影到 `util::Errc` 之前，至少要先满足：

- backend 错误能被分类
- driver 不只拿到 `bool`、裸整数或 vendor status
- 同一错误能在 artifact / compare / inspector 中被解释

candidate taxonomy 至少应考虑：

- `bus`
- `mode_fault`
- `overrun`
- `chip_select_fault`
- `timeout`
- `target_detached`
- `policy_violation`
- `unsupported`
- `unknown`

在 `experimental` 之前，不应为了单一 backend 草率扩展公共错误 taxonomy。

## 6. Facts / Evidence / Compare Checklist

SPI proposed contract 未来至少需要能投影下面 facts：

- `spi.bus`
- `spi.controller`
- `spi.device`
- `spi.chip_select`
- `clock.domain`
- `pinmux`
- `power.domain`
- `dma.channel`
- `spi.backend`
- `spi.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- `system_compiler.fact_evidence/v0`
- artifact report
- compare report
- explain / unresolved binding 入口

最低证据链建议至少准备：

- no-hardware mock evidence
- `fact_evidence` sidecar
- artifact report 投影
- baseline / compare 入口

如果后续出现 Host fixture 或真实 board/probe evidence，再把 producer-side compare 接上。

## 7. Readiness Gate

当且仅当下面条件至少大部分满足时，SPI 才值得继续往 `experimental` 方向推进：

- `SpiDevice` 事务边界已经能被 mock 清楚表达
- driver 不再直接管理 CS
- 错误语言至少能落到一组稳定分类
- `fact_evidence` / artifact / compare 链路已经能消费 SPI 证据
- 有一条明确的 no-hardware baseline

只要这些还没闭环，SPI 就继续保持 `proposed`。

## 8. 当前非目标

本清单当前不做：

- 不新增 C++ API
- 不修改 `hal_spi`
- 不修改 `block.spi_flash`
- 不新增 schema
- 不修改 Examples
- 不修改 CMake
- 不要求 QEMU 或真实板运行
- 不把 SPI 升级为 `experimental`
- 不把 CS 暴露为每个 driver 的私有 GPIO 手艺活

## 9. 当前推荐下一步

下一步真正开第一张票时，优先把这份清单当作准入台账：

1. 先把 `SpiDevice` transaction mock 的脚本语义固定住。
2. 先补 no-hardware mock evidence。
3. 再补 `system_compiler.fact_evidence/v0` sidecar。
4. 再接 artifact report 与 compare baseline。
5. 最后再决定是否需要更小的准真实 SPI driver 样板。

这份清单的作用是让 SPI 的第一张票先可描述、可比较、可解释，再谈实现。
