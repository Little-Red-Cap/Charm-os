# Block Device Fault Script Readiness Checklist v0

本文是 Block proposed contract 进入 mock / fault script 之前的准备清单。

它不是新的契约，不是新的 API，也不是 Block `experimental` 升级申请。

它只回答一个问题：

> **一条 block fault script evidence 在进入 `artifact_report` / `compare` / evidence 链路前，应该先准备好什么。**

上位入口：

- [`../../architecture/block_device_contract_v0.md`](../../architecture/block_device_contract_v0.md)
- [`../../architecture/interface_admission_policy.md`](../../architecture/interface_admission_policy.md)

## 1. 当前定位

当前 Block 仍是 `proposed`。

它已经有：

- `fs::BlockDevice` 形状
- `block.registry` 的 capability 装配经验
- `DeviceSlotExport` 的 runtime slot 经验
- file-backed block backend
- cache proxy
- SPI flash binding 经验

但这些都还不是 driver-facing block mock / fault script evidence。

这份清单只负责把第一张票的准备项固定住，不负责直接实现 mock。

## 2. Producer / Source / Subject / Facets Checklist

Block mock producer 接入前，必须先能回答：

- `source`
  evidence 来自哪个 example、脚本、host fixture 或 mock backend。
- `producer`
  producer 名称必须能区分 mock、Host fixture、准真实或真实 storage target。
- `subject`
  必须明确 block 目标对象。
  至少要说明是 `sd0`、`flash0`、`ram0`、`usb0` 或其他 block endpoint。
- `active_facets`
  至少应能表达 `platform`、`board_package`、`storage`、`block_contract`、`fault_script`、`evidence`、`compare` 中实际参与的 facet。
- `host_fixture`
  如果仍然使用 host/mock transaction，必须在摘要里诚实标出。
- `real_hardware`
  如果声明为真实硬件 evidence，必须能追溯到真实板级运行记录或真实 probe 输出。

推荐 producer 命名形态：

```text
block.fault_script.<target_name>
```

如果后续要接 Host fixture 或真实板级 evidence，再另起更明确的 producer 名称。

## 3. Block Backend / Endpoint 边界

这份清单默认 Block 仍按 backend / endpoint 两层收敛：

- `BlockDevice`
- `BlockEndpoint`
- `BlockMediaState`

`BlockDevice` 表示一个 LBA-oriented block backend。

`BlockEndpoint` 表示对外可消费的 capability endpoint。

`BlockMediaState` 表示 runtime 或 removable media 的存活状态。

推荐理解：

```text
driver -> BlockEndpoint -> managed block backend
```

而不是：

```text
driver -> file system policy + cache policy + private backend handle
```

backend 不应该泄漏：

- vendor SDK handle
- 文件系统私有对象
- USB BOT / SCSI 内部阶段
- SPI controller 私有事务
- board 私有存储句柄

## 4. Fault Script Coverage Checklist

第一张 Block fault script 票至少应覆盖：

- read success
- read fault
- write success
- write fault
- erase success
- erase fault
- flush success
- flush fault
- media missing
- detach
- write protect
- invalid geometry

这张票不需要一开始就覆盖所有 storage 变体。

暂不覆盖：

- partition parser
- filesystem policy
- cache replacement policy
- DMA-safe buffer
- async storage
- managed time / replay
- timeout 托管

fault script 语义最重要的是说明：

- backend 能按序消费脚本
- media state 的变化是显式的
- detach 后再访问应能返回明确状态
- write protect 不应被当作普通 read/write 成功

## 5. Media State Language Checklist

Block 的 media state language 必须把“端点可见性”和“介质可用性”分开。

候选状态至少应考虑：

- `missing`
- `detached`
- `attached`
- `present`
- `write_protected`
- `failed`

这份语言应能解释：

- endpoint 是否已发布
- media 是否真的存在
- media 是否可写
- runtime slot 是不是已经脱离

它不应该和 filesystem policy、cache policy 或 VFS mount state 混成一层。

## 6. Error Semantics Checklist

Block 公共错误语言尚未冻结。

在投影到 `util::Errc` 之前，至少要先满足：

- backend 错误能被分类
- driver 不只拿到 `bool`、裸整数或 vendor status
- 同一错误能在 artifact / compare / inspector 中被解释

candidate taxonomy 至少应考虑：

- `invalid_geometry`
- `out_of_range`
- `read_fault`
- `write_fault`
- `erase_fault`
- `flush_fault`
- `media_missing`
- `target_detached`
- `write_protected`
- `unsupported`
- `policy_violation`
- `timeout`
- `unknown`

现有经验可以映射为：

- detach 后访问返回 `Errc::noent`
- 缺失 write / erase 返回 `Errc::nosys`
- cache bind 到只读设备返回 `Errc::rofs`
- invalid block geometry 返回 `Errc::invalid_arg` 或 `Errc::inval`

在 `experimental` 之前，不应为了单一 backend 草率扩展公共错误 taxonomy。

## 7. Facts / Evidence / Compare Checklist

Block proposed contract 未来至少需要能投影下面 facts：

- `block.device`
- `block.endpoint`
- `block.backend`
- `block.geometry`
- `block.media`
- `block.partition`
- `block.cache`
- `storage.controller`
- `clock.domain`
- `power.domain`
- `dma.channel`
- `block.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- `system_compiler.fact_evidence/v0`
- artifact report
- compare report
- explain / unresolved binding 入口
- bringup block order 与 runtime detach 解释

最低证据链建议至少准备：

- no-hardware mock evidence
- `fact_evidence` sidecar
- artifact report 投影
- baseline / compare 入口

如果后续出现 Host fixture 或真实 board/probe evidence，再把 producer-side compare 接上。

## 8. Readiness Gate

当且仅当下面条件至少大部分满足时，Block 才值得继续往 `experimental` 方向推进：

- backend / endpoint 边界已经能被 mock 清楚表达
- fault script 能按序表达 read / write / erase / flush / detach / protect 变化
- media state language 已经稳定
- 错误语言至少能落到一组稳定分类
- `fact_evidence` / artifact / compare 链路已经能消费 Block 证据
- 有一条明确的 no-hardware baseline

只要这些还没闭环，Block 就继续保持 `proposed`。

## 9. 当前非目标

本清单当前不做：

- 不新增 C++ API
- 不修改 `fs_block`
- 不修改 `block.registry`
- 不修改 `DeviceSlotExport`
- 不修改 VFS / FatFs / USB MSC
- 不新增 schema
- 不修改 Examples
- 不修改 CMake
- 不要求 QEMU 或真实板运行
- 不把 Block 升级为 `experimental`
- 不把 filesystem policy 塞进 block contract
- 不把 cache policy 塞进 block contract
- 不承诺 DMA / async / timeout / managed time
- 不把 runtime discovery 反向变成 block 静态装配主模型

## 10. 当前推荐下一步

下一步真正开第一张票时，优先把这份清单当作准入台账：

1. 先把 block mock / fault script 的语义固定住。
2. 先把 `BlockMediaState` 的状态语言固定住。
3. 再建立 contract-local block facts 草案。
4. 再补 no-hardware mock evidence。
5. 最后再决定是否需要更小的准真实 storage driver 样板。

这份清单的作用是让 Block 的第一张票先可描述、可比较、可解释，再谈实现。
