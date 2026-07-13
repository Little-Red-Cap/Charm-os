# USB 架构概览

## 文档状态

- `status`: `supporting`
- `scope`: USB module 分组、当前验证范围与未证明边界
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

USB 是 IO 专题实现，不定义 Charm Core。具体类型以 `Modules/io/usb` 源码为准，验证入口以
[`Examples/usb/README.md`](../../Examples/usb/README.md) 为准。

## 当前分层

| 范围 | module | 责任 |
|---|---|---|
| common | `usb.common`、`usb.model`、`usb.spec`、`usb.dsl`、`usb.plan`、`usb.runtime` | 描述符、规格投影与平台无关模型 |
| device | `usb.device`、`usb.device_driver`、`usb.ep0_driver`、`usb.class_mux` | EP0、标准请求、class 分派与 device driver 边界 |
| class | `usb.cdc`、`usb.msc`、`usb.msc_block`、`usb.uac` 及 node modules | class 状态、storage bridge 与装配节点 |
| host | `usb.host.core`、`usb.host.runtime*` | discovery 到 runtime registry、block/channel binding 与 manager |
| mock | `usb.fixture`、`usb.mock`、`usb.replay`、`usb.boardlog` | host fixture、trace replay 与板级日志导入 |

Descriptor/common 不依赖具体 DCD/HCD。Class 通过 device/host 边界消费 control/data 事件；平台
driver 负责把真实控制器的端点、完成与错误事件映射进这些边界。

## 已有证据

- Device 侧有 CDC、MSC、MSC+CDC 的 native mock/replay；`usb_cdc_minimal` 只构建 descriptor tree
  并调用 class buffer callback，不执行 EP0、DCD 或 host enumeration。
- Host 侧有 discovery、runtime manager、block/channel export 与 remove/rediscover 的 host smoke。
- Boardlog importer 验证日志格式到 replay 的工具链，不等于真实板实时执行。
- UAC 当前只有 module/descriptor 基础面，没有同等级 runtime smoke，不能从文件存在推断音频闭环。

统一 native 入口是：

```powershell
./scripts/usb_native_smoke.ps1
```

测试 target 与工具链参数以各目录 CMake/source 和 runner 为准，本文不复制清单。

## 未证明

- native fixture 不证明 DCD/HCD IRQ、cache、DMA、端点时序或主机兼容性；
- minimal example 不证明产品级 CDC console、MSC filesystem 或 UAC streaming；
- host runtime smoke 不证明真实 USB Host controller、hub、热插拔并发或完整 class 覆盖；
- descriptor 构建成功不证明设备可以枚举或 class data path 可用。

EP0、CDC、descriptor/string、mock/replay 和 boardlog 的局部规则从
[`README.md`](README.md) 进入对应 contract、overview 或 workflow。
