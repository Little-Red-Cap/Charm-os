# 架构文档入口

本目录收纳 Charm 的依赖红线、驱动模型、signal/state、能力回收，以及 system compiler 相关材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按主题进入。

## 先怎么理解这个目录

- `*_contract.md`
  优先视为现行规则或当前阶段最值得先遵守的边界。
- `*_overview.md`
  优先视为主题入口或草案总览。
- `*_roadmap.md`
  优先视为中长期方向和推进主轴。
- `*_v0.md`
  优先视为阶段性词汇、原语或阶段收口快照。

## 按主题进入

### 我想看依赖红线和分层边界

先读：

- [`dependency_contract.md`](dependency_contract.md)
- [`dependency_whitelist.md`](dependency_whitelist.md)

### 我想看驱动模型和设备模型

先读：

- [`driver_model.md`](driver_model.md)
- [`interface_admission_policy.md`](interface_admission_policy.md)（公共接口进入契约层前的证据门槛）
- [`device_contract_narrow_waist_v0.md`](device_contract_narrow_waist_v0.md)（面向驱动生态的最小共同语言）
- [`device_model_overview.md`](device_model_overview.md)（偏动态 discovery 平面快照与补充说明）

### 我想看公共接口契约 / 驱动生态窄腰

建议顺序：

1. [`driver_model.md`](driver_model.md)
2. [`interface_admission_policy.md`](interface_admission_policy.md)
3. [`device_contract_narrow_waist_v0.md`](device_contract_narrow_waist_v0.md)
4. [`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)

当前第一条实验窄链：

- `Modules/io/device/io.device_i2c.cppm`
- `Modules/io/device/io.device_i2c_mock.cppm`
- `Examples/io/i2c_contract_mock_smoke`

### 我想看 signal / state 这条线

建议顺序：

1. [`signal_state_contract_v0.md`](signal_state_contract_v0.md)
2. [`signal_state_v0.md`](signal_state_v0.md)

### 我想看 system compiler 主线

建议顺序：

1. [`charm_methodology_charter.md`](charm_methodology_charter.md)
2. [`system_compiler_roadmap.md`](system_compiler_roadmap.md)
3. [`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)

如果你继续往运行面和工件面走，再回到：

- [`../system/README.md`](../system/README.md)
- [`../../schemas/README.md`](../../schemas/README.md)

### 我想看外部成熟产品对 Charm 机制的启发

读：

- [`tdesktop_mechanism_lessons_for_charm.md`](tdesktop_mechanism_lessons_for_charm.md)

### 我想看能力回收

读：

- [`capability_recovery_rules.md`](capability_recovery_rules.md)
- [`capability_recovery_matrix.md`](capability_recovery_matrix.md)

### 我在排查 C++ 模块与标准库链接问题

读：

- [`cpp_modules_stdlib_linkage_conflicts.md`](cpp_modules_stdlib_linkage_conflicts.md)

## 当前建议阅读顺序

- 看总架构和方法论：
  `charm_methodology_charter.md`
- 看依赖与分层：
  `dependency_contract.md`
- 看驱动与设备：
  `driver_model.md` → `interface_admission_policy.md` → `device_contract_narrow_waist_v0.md` → `device_model_overview.md`（后者偏动态 discovery 平面与历史补充）
- 看 signal/state：
  `signal_state_contract_v0.md`
- 看 system compiler：
  `system_compiler_roadmap.md` → `system_compiler_vocabulary_v0.md`

## 使用提醒

- 如果这里的说明和 `docs/system/*`、`docs/io/*` 或当前代码冲突，优先回到更上位入口复核。
- 如果你只想抓“当前主结论”，优先先停在 `driver_model.md`，不要直接从历史对照段落建立认知。
- 当分层规则、驱动模型、signal/state 契约或 system compiler 公开主线变化时，应同步更新本目录入口。
