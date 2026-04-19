# USB 文档入口

本目录收纳 Charm USB 路线当前与 Device / Host、描述符 DSL、CDC 契约、mock/replay 工作流、boardlog 和调试建议相关的材料。

如果你想先看示例和验证入口，再看细节，也可以同时打开：

- [`../../Examples/usb/README.md`](../../Examples/usb/README.md)

## 先怎么理解这个目录

- `*_overview.md`
  优先视为 USB 主题入口、DSL 或装配说明。
- `*_contract.md`
  优先视为现行最小接口契约。
- `*_workflow.md`
  优先视为当前推荐验证流程。
- `*_format.md` / `*_vocabulary.md` / `*_matrix.md`
  优先视为 replay、boardlog 或回归语义的稳定输入面。
- `*_guidelines.md`
  优先视为建议性调试经验，不自动等同于硬契约。

## 按任务进入

### 我想先理解 USB 现在在做什么

先读：

- [`usb_arch_plan.md`](usb_arch_plan.md)
- [`usb_dsl_overview.md`](usb_dsl_overview.md)

### 我想看当前推荐的验证方式

先读：

- [`usb_native_mock_workflow.md`](usb_native_mock_workflow.md)
- 再配合 [`../../Examples/usb/README.md`](../../Examples/usb/README.md)

### 我想接入或排查 CDC

读：

- [`usb_cdc_contract.md`](usb_cdc_contract.md)

### 我想看 string/lang 装配

读：

- [`usb_strings_overview.md`](usb_strings_overview.md)

### 我想看 boardlog / replay 这条线

建议顺序：

1. [`usb_boardlog_format.md`](usb_boardlog_format.md)
2. [`usb_boardlog_coverage_matrix.md`](usb_boardlog_coverage_matrix.md)
3. [`usb_msc_trace_vocabulary.md`](usb_msc_trace_vocabulary.md)

### 我在调 USB Audio

读：

- [`usb_audio_debug_guidelines.md`](usb_audio_debug_guidelines.md)

## 当前建议阅读顺序

- 看总规划：
  `usb_arch_plan.md`
- 看描述符与装配：
  `usb_dsl_overview.md` → `usb_strings_overview.md`
- 看验证工作流：
  `usb_native_mock_workflow.md`
- 看具体类契约：
  `usb_cdc_contract.md`
- 看 boardlog / replay：
  `usb_boardlog_format.md` → `usb_boardlog_coverage_matrix.md` → `usb_msc_trace_vocabulary.md`

## 使用提醒

- USB 这条线同时有“设计说明”“建议性调试经验”“稳定 replay 输入面”三类文档，阅读时不要混成一层。
- 如果这里的说明和 `Examples/usb/*` 的当前验证入口冲突，优先回到示例入口和当前脚本链复核。
