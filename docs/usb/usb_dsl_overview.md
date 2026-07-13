# USB Descriptor 与 Plan 流程

## 文档状态

- `status`: `supporting`
- `scope`: USB descriptor builder 与 device planning 的当前边界
- `source`: `Modules/io/usb/common/usb.{dsl,spec,model,plan,runtime}.cppm`

仓库有两层相关接口。它们不是两套 USB runtime：plan/runtime 最终仍使用底层 DSL 生成 descriptor
并建立 device binding。

## Direct DSL

`usb.dsl` 提供 `ConfigBuilder`、`DeviceBuildContext` 与 `build_*_device()`：

- 支持 CDC ACM、MSC、MSC+CDC 和 UAC descriptor；
- 写入调用方提供的 device/config byte buffer；
- 填充 `device::DescriptorTable` 与 `device::ConfigTree`；
- 检查 descriptor stream、EP0 占用和 endpoint address 冲突。

这些函数返回 `bool`，不会指出具体失败字段。buffer、class descriptor span、string table 及其引用对象
均由调用方管理生命周期。

## Spec 到 Runtime

当前 native examples 主要使用：

```text
usb.spec -> usb::build(model) -> usb::plan::build(plan) -> usb::runtime::make(binding)
```

| 层 | 责任 |
|---|---|
| `usb.spec` | 描述 Device、CDC、MSC 或 MSC+CDC 的输入值和 capability name |
| `usb.model` | 生成 interface/endpoint intent 与 class config |
| `usb.plan` | 分配 interface/endpoint，检查冲突和 target constraints |
| `usb.runtime` | 将 plan、DCD/runtime config 和可选 block registry 组成 init binding |

`usb.plan` 默认使用 `stm32_fs_constraints()`：EP0 MPS、bulk MPS、interface number 和 endpoint number
都有上限。失败使用 `invalid_arg`、`exist` 或 `buffer_overflow`，没有逐字段诊断。

## 边界

- Spec/plan 当前覆盖 CDC、MSC 和 MSC+CDC；UAC 只有 direct DSL 路径。
- `DeviceSpec::strings`、capability name 和 MSC identity 使用借用的 span/pointer，使用期间必须有效。
- plan 成功只说明当前模型满足分配约束，不证明 DCD 可用、descriptor 能枚举或 data path 正常。
- Direct DSL 成功只说明固定 buffer 中生成了结构可接受的 descriptor/tree。
- 默认 FS constraints 不是 USB HS 或任意控制器的通用限制；平台若不同必须显式提供 constraints。

当前组合用法和 smoke 入口见 [`Examples/usb/README.md`](../../Examples/usb/README.md)。String/lang
装配见 [`usb_strings_overview.md`](usb_strings_overview.md)。
