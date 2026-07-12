# GPIO Device Input/Output/Edge Readiness Checklist v0

本文是 GPIO proposed contract 进入 mock/backend evidence 之前的准备清单。

它不是新的契约，不是新的 API，也不是 GPIO `experimental` 升级申请。

它只回答一个问题：

> **一条 GPIO mock evidence 在进入 `artifact_report` / `compare` / evidence 链路前，应该先准备好什么。**

上位入口：

- [`../../architecture/gpio_device_contract_v0.md`](../../architecture/gpio_device_contract_v0.md)
- [`../../architecture/interface_admission_policy.md`](../../architecture/interface_admission_policy.md)

## 1. 当前定位

当前 GPIO 仍是 `proposed`。

它已经有：

- controller-facing `hal_gpio`
- input 分层边界
- `GpioInput / GpioOutput / GpioEdgeSource` 的窄腰概念

但这些都还不是 driver-facing GPIO mock evidence。

这份清单只负责把第一张票的准备项固定住，不负责直接实现 mock。

## 2. Producer / Source / Subject / Facets Checklist

GPIO mock producer 接入前，必须先能回答：

- `source`
  evidence 来自哪个 example、脚本、host fixture 或 mock backend。
- `producer`
  producer 名称必须能区分 mock、Host fixture、准真实或真实 board/probe。
- `subject`
  必须明确 GPIO 目标对象。
  至少要说明是 `LED`、`button`、`edge counter` 或其他 pin-level subject。
- `active_facets`
  至少应能表达 `platform`、`board_package`、`io`、`driver_contract`、`gpio_mock`、`evidence`、`compare` 中实际参与的 facet。
- `host_fixture`
  如果仍然使用 host/mock transaction，必须在摘要里诚实标出。
- `real_hardware`
  如果声明为真实硬件 evidence，必须能追溯到真实板级运行记录或真实 probe 输出。

推荐 producer 命名形态：

```text
gpio.mock.<target_name>
```

如果后续要接 Host fixture 或真实板级 evidence，再另起更明确的 producer 名称。

## 3. GpioInput / GpioOutput / GpioEdgeSource 边界

这份清单默认 GPIO 仍按三层语义拆分：

- `GpioInput`
- `GpioOutput`
- `GpioEdgeSource`

`GpioInput` 表示读取当前电平。

`GpioOutput` 表示设置输出电平。

`GpioEdgeSource` 表示发布边沿事件来源。

推荐理解：

```text
GpioInput       -> read current level
GpioOutput      -> set output level
GpioEdgeSource  -> publish edge occurrence
```

公共 GPIO contract 不应该把 pin 做成万能对象。

## 4. Mock Coverage Checklist

第一张 GPIO mock 票至少应覆盖：

- read level
- write level
- edge occurrence script
- invalid pin
- direction mismatch
- not configured
- target detached
- backend failure

这张票不需要一开始就覆盖所有 GPIO 变体。

暂不覆盖：

- debounce
- click / long press
- repeat
- UI intent
- input routing
- polling scheduler
- managed time / replay

mock 语义最重要的是说明：

- driver 不手动承担 input.service 语义
- transaction 返回前 pin 处于定义状态
- edge source 只投递 edge occurrence，不执行完整上层逻辑

## 5. Error Semantics Checklist

GPIO 公共错误语言尚未冻结。

在投影到 `util::Errc` 之前，至少要先满足：

- backend 错误能被分类
- driver 不只拿到 `bool`、裸整数或 vendor status
- 同一错误能在 artifact / compare / inspector 中被解释

candidate taxonomy 至少应考虑：

- `unsupported`
- `invalid_pin`
- `direction_mismatch`
- `not_configured`
- `target_detached`
- `policy_violation`
- `io_fault`
- `unknown`

在 `experimental` 之前，不应为了单一 backend 草率扩展公共错误 taxonomy。

## 6. Facts / Evidence / Compare Checklist

GPIO proposed contract 未来至少需要能投影下面 facts：

- `gpio.controller`
- `gpio.pin`
- `gpio.input`
- `gpio.output`
- `gpio.edge_source`
- `pinmux`
- `pull`
- `irq.line`
- `power.domain`
- `gpio.backend`
- `gpio.evidence`

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

当且仅当下面条件至少大部分满足时，GPIO 才值得继续往 `experimental` 方向推进：

- `GpioInput / GpioOutput / GpioEdgeSource` 边界已经能被 mock 清楚表达
- driver 不再把 debounce 或 UI intent 塞进基础 pin contract
- 错误语言至少能落到一组稳定分类
- `fact_evidence` / artifact / compare 链路已经能消费 GPIO 证据
- 有一条明确的 no-hardware baseline

只要这些还没闭环，GPIO 就继续保持 `proposed`。

## 8. 当前非目标

本清单当前不做：

- 不新增 C++ API
- 不修改 `hal_gpio`
- 不修改 `input.service`
- 不修改 input sampler
- 不新增 schema
- 不修改 Examples
- 不修改 CMake
- 不要求 QEMU 或真实板运行
- 不把 GPIO 升级为 `experimental`
- 不把 debounce 放进基础 pin contract
- 不把 UI intent 放进 GPIO contract
- 不把 pin 做成万能对象

## 9. 当前推荐下一步

下一步真正开第一张票时，优先把这份清单当作准入台账：

1. 先把 `GpioInput / GpioOutput / GpioEdgeSource` 的职责卡固定住。
2. 先补 no-hardware mock evidence。
3. 再补 `system_compiler.fact_evidence/v0` sidecar。
4. 再接 artifact report 与 compare baseline。
5. 最后再决定是否需要更小的准真实 GPIO driver 样板。

这份清单的作用是让 GPIO 的第一张票先可描述、可比较、可解释，再谈实现。
