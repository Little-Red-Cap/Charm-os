# Block Device Contract v0

## 定位

本文记录 Charm 设备契约窄腰中的 Block Device proposed contract card。

它不是 admitted 公共 ABI，也不是当前
[`../../storage/block_device_contract.md`](../../storage/block_device_contract.md)
的替代品。

它只回答一个更窄的问题：

> **一个 block 后端、文件系统桥接或 runtime storage glue 如果要成为可复用设备契约，未来最小应该承诺哪些 sector、live state、flush、错误和 facts 语义。**

当前代码中已经存在：

- `Modules/io/fs/fs_block.cppm`
- `Modules/io/block/block.device.cppm`
- `Modules/io/block/block.registry.cppm`
- `Modules/io/block/block.device.node.cppm`
- `Modules/io/block/block.device.slot.cppm`
- `Modules/io/block/block.device.slot_export.cppm`
- `Modules/io/block/block.file.cppm`
- `Modules/io/block/block.cache.cppm`
- `Modules/io/block/block.spi_flash.cppm`

这些说明 Charm 已经有比较扎实的 block 装配经验。

但它们仍然不等价于一个已经 admitted 的公共 driver-facing block device contract。

## 1. 当前等级

旧流程将它标为 `proposed`；这只是历史本地标签，不是 Constitution 裁决。

它已经具备：

- storage-facing `fs::BlockDevice` 形状
- `block::Caps` 能力位：`read / write / erase / flush / cached`
- `block.registry` 的唯一 capability 注册与 `PublishState`
- `block::DeviceBinding` 的 init.graph 静态装配入口
- `block::DeviceSlot` 的稳定转发槽位
- `block::DeviceSlotExport` 的 `missing / detached / attached` 导出状态
- file-backed block backend
- cache proxy
- SPI flash binding 经验
- VFS / FatFs / USB MSC / runtime slot 多条 smoke 或 demo 路径

它还不是 `experimental`，因为仍然缺：

- 面向 driver / component 作者的正式 block contract 责任卡
- 专门服务公共契约的 block mock backend 或 fault script
- contract-local block facts vocabulary
- 更窄的 block domain error taxonomy
- media present / write protect / erase granularity 的统一表达
- artifact / evidence pipeline 中正式的 facts 投影
- 一个只依赖该 contract 的准真实 storage driver evidence

## 2. Contract Shape

当前 v0 不新增 C++ API。

Block proposed contract 的最小语义面应围绕下面对象收敛：

- `BlockDevice`
- `BlockEndpoint`
- `BlockMediaState`

`BlockDevice` 表示一个 LBA-oriented block backend。

它至少需要表达：

- `block_size`
- `block_count`
- `read(lba, buffer)`
- `write(lba, buffer)`
- `erase(lba, count)`
- `flush()`
- capability bits

`BlockEndpoint` 表示对外可消费的 capability endpoint，例如：

- `block.sd0`
- `block.flash0`
- `block.usb0`
- `block.ram0`

`BlockMediaState` 表示 runtime 或 removable media 的存活状态，例如：

- `missing`
- `detached`
- `attached`
- `present`
- `write_protected`
- `failed`

当前仓库里的 `fs::BlockDevice`、`block.registry` 与
`DeviceSlotExport` 已经分别提供了这些方向的胚胎。
但 proposed contract 仍需要把它们收束成面向公共设备契约的准入记录。

## 3. Ownership And Responsibility

### 3.1 Block Backend

Block backend 负责：

- 提供固定 block size 与 block count
- 把 LBA 转换成后端真实地址或请求
- 完成 read / write / erase / flush
- 把平台错误映射成公共错误语言
- 在可移除或动态设备失活时返回明确状态

Block backend 不应该泄漏：

- vendor SDK handle
- 文件系统私有对象
- USB BOT / SCSI 内部阶段
- SPI controller 私有事务
- board 私有存储句柄

### 3.2 Block Endpoint

Block endpoint 负责：

- 用 capability name 暴露稳定入口
- 保证同名同 cap 不重复注册
- 对动态后端保持稳定指针或稳定槽位
- 在后端 detach 后避免悬挂指针
- 让上层通过 registry / capability 查找，而不是理解后端来源

当前 `DeviceSlotExport` 已经验证了重要方向：

```text
runtime device attach -> stable block endpoint -> runtime device detach
                         -> old pointer remains safe and returns noent
```

这条经验很接近 runtime block endpoint contract，但仍需要进入公共准入语言。

### 3.3 Filesystem / VFS Layer

Filesystem / VFS layer 负责：

- 从 block endpoint 打开设备
- 解释 partition / FAT / blockfs 等文件系统语义
- 管理 mount prefix、path routing 与 file lifecycle
- 决定何时需要强一致 flush

Filesystem / VFS layer 不应该反向决定 block contract 的基础语义。

例如：

- `vfs_close` 是否强制落盘属于 VFS 策略
- block device 只需要说明 `flush` 是否存在、调用后承诺什么

### 3.4 Cache Proxy

Cache proxy 负责：

- 用缓存策略封装底层 block backend
- 暴露新的 block device view
- 标记 `Caps::cached`

Cache proxy 不应该改变：

- 底层 sector size 语义
- LBA 语义
- media live state 的基本解释
- read/write/flush 的错误归类

## 4. Sector And Media Semantics

Block contract 必须比普通 `read/write` 函数更明确。

至少需要记录：

- `block_size` 是否非零
- `block_count` 是否非零
- `lba` 是否按 block 计数，不是 byte address
- buffer 长度是否必须是 block size 的整数倍
- read/write 是否允许跨多个 block
- erase granularity 是否等于 block size
- flush 是否代表写缓存落盘或只是 backend fence
- media 是否可能运行期消失
- write protect 如何表达

当前代码已经要求静态 `DeviceBinding` 注册前具备：

- `block_size != 0`
- `block_count != 0`
- `read != nullptr`

但这只是现有装配约束，还不是完整公共 block contract。

## 5. Execution Semantics

当前 Block proposed contract 暂定为同步完成模型。

一次调用返回时，backend 应完成下列之一：

- 操作成功完成
- 操作明确失败并返回公共错误
- backend 表示当前能力不支持
- media / endpoint 当前不可用

当前不承诺：

- ISR-safe
- reentrant
- non-blocking
- reactor-managed
- DMA-safe buffer
- timeout 由公共 contract 托管
- managed time / replay 可控制

如果未来需要 async storage、DMA 或 timeout，必须通过明确 reactor / timebase / resource contract 进入。

## 6. Error Semantics

当前 Block 路径主要复用 `fs::Errc` / `util::Errc`。

这比 `bool` 风格更好，但 proposed contract 仍缺一组更窄的 block domain error taxonomy。

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

在 `experimental` 前，不应为了某个单一后端草率冻结 taxonomy。

## 7. Facts

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
- artifact report
- evidence sample
- explain / unresolved binding 入口
- bringup block order 与 runtime detach 解释

## 8. Evidence Inventory

当前已有的 block 证据主要分成四类。

### 8.1 Static Capability And Registry Evidence

已有：

- `block.registry`
- `DeviceBinding`
- `RegistryBinding`
- 唯一 cap / name 注册
- `PublishState::missing / published`

这证明 block endpoint 可以进入 capability / init.graph 装配语言。

### 8.2 Runtime Slot Evidence

已有：

- `DeviceSlot`
- `DeviceSlotExport`
- `ExportState::missing / detached / attached`
- attach / detach / unexport transition observer
- `device_runtime_block_slot_demo`
- `usb_host_runtime_block_smoke`

这证明 runtime discovered storage 可以通过稳定 block capability 对外收口。

### 8.3 Storage Integration Evidence

已有：

- file-backed block device
- cache proxy
- SPI flash binding
- VFS / FatFs / blockfs demo 路径
- USB MSC block demo

这证明 block abstraction 已经被多个上层消费。

### 8.4 Observe / Artifact Evidence

已有：

- `bringup_block_observe_demo`
- `usb_msc_block_demo` 的 runtime observe sidecar
- materialized graph / runtime observe 导出经验

这证明 block endpoint 状态可以进入观察与工具消费链。

但这些仍然是系统装配和运行观察证据。
它们不等价于一张已经完成准入的 driver-facing block contract。

## 9. Evidence Gaps

当前缺口明确保留：

- 没有专门的 block contract mock / fault script
- 没有 contract-local facts vocabulary
- 没有 block domain error kind
- 没有统一 media present / write protect 状态
- 没有 erase granularity contract
- 没有 DMA-safe buffer / cache coherency 资源语义
- 没有真实硬件 bringup evidence 与 contract facts 的组合报告
- 没有把 block facts 正式投影进 artifact report 的样例

这些缺口补齐前，Block 仍保持 `proposed`。

## 10. Non-goals

当前阶段明确不做：

- 不新增 `Modules/io/device/io.device_block.cppm`
- 不修改 `fs_block`
- 不重构 `block.registry`
- 不重构 `DeviceSlotExport`
- 不修改 VFS / FatFs / USB MSC
- 不宣布 Block contract 为 `experimental`
- 不把 filesystem policy 塞进 block contract
- 不把 cache policy 塞进 block contract
- 不承诺 DMA / async / timeout / managed time
- 不把 runtime discovery 反向变成 block 静态装配主模型

## 11. Next Steps

最值当的下一步是：

1. 保持本文件为 `proposed` card。
2. 先按 [`../../system/block_device_fault_script_readiness_checklist_v0.md`](../../system/block_device_fault_script_readiness_checklist_v0.md) 收拢 producer / source / subject / facts / evidence 语义，但不急着写代码。
3. 先设计 block mock / fault script 的语义，例如 read fault、write protect、detach、flush fault。
4. 把 `DeviceSlotExport` 的 transition 经验投影成更正式的 block media state language。
5. 建立 contract-local block facts 草案，只做报告，不做执法。
6. 选择一个准真实 storage driver evidence，例如 EEPROM block adapter、SPI NOR ID + block view、RAM disk with fault injection。
7. 按 [`../../architecture/interface_admission_policy.md`](../../architecture/interface_admission_policy.md) 记录新增实现与证据。

在这些完成前，Block 仍保持 `proposed`。
