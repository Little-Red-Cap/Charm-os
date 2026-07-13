# Vivid 静态内存准入

状态：supporting implementation contract。本文只约束 Vivid domain，不定义 Charm Core 词汇。

## 目标

`PRODUCT` 与 `MCU_MIN` 必须在配置时声明 Vivid 常驻 RAM 预算和最小余量。配置模型先给出保守上界，目标编译器再用真实 ABI 的 `sizeof` 验证上界没有低估；任一层失败都禁止进入产品构建。

## 预算范围

计入：

- 配置声明数量的 `Scene` 实例；
- `SoaKernel`、payload pools、TextArena；
- 唯一 live DrawCmd buffer 与每个 layer slot 的 command/pixel snapshot storage；
- DrawCmd compaction、executor 以及 SoA render/layout/input/semantic traversal workspace；
- Theme、ThemeTokens、StyleSheet 和 ImageRegistry 常驻状态；
- 已启用 widget 的 object-style slot 保守储备与 style state 可变表。

不计入：

- 端口提供的 Canvas/framebuffer、显示控制器显存；
- 字体和图片的只读资源数据；
- 应用自身对象、任务栈、驱动 DMA buffer；
- 通过栈门约束但不属于常驻 RAM 的小型函数局部变量。

因此该门证明的是“Vivid 自有常驻 RAM 在分配给 Vivid 的预算内装得下并保留余量”，不是整机 RAM 总账。

## 配置

`PRODUCT` 与 `MCU_MIN` 必须显式设置：

- `CHARM_VIVID_RUNTIME_SCENE_INSTANCES`
- `CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES`
- `CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES`

DrawCmd 与热路径栈画像由以下配置控制：

- `CHARM_VIVID_DRAW_CMD_MAX_COMMANDS`
- `CHARM_VIVID_DRAW_CMD_TEXT_BYTES`
- `CHARM_VIVID_DRAW_CMD_BLOB_BYTES`
- `CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES`

默认 DrawCmd profile 为 `1024/4096/2048`，默认 Vivid 栈帧上限为 `4096B`。PRODUCT profile 应显式固定这些值；容量下调必须有真实产品峰值证据。

三项都不能依赖隐式默认值；Scene 实例数、预算和最小余量都必须大于零。

`FULL` 生成画像但默认不执行预算准入。配置证据写入 Vivid 当前 binary dir：

```text
${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/static_memory_admission.txt
```

例如 `soa_demo` 将 Vivid 作为 `Charm` 子项目引入时，实际路径是
`<build>/Charm/generated/vivid/static_memory_admission.txt`。

配置期上界必须满足：

```text
upper_bound_bytes + min_headroom_bytes <= budget_bytes
```

编译期还必须满足：

```text
target_abi_exact_bytes <= upper_bound_bytes
target_abi_exact_bytes + min_headroom_bytes <= budget_bytes
```

开启 `CHARM_VIVID_MEMORY_PROFILE_SYMBOLS=1` 时，GNU 兼容 MCU 目标文件提供
`charm_vivid_static_profile_*` absolute symbols，供最终 ELF/map evidence 读取真实 ABI 数值。
画像单列 live buffer 数量、compaction workspace、executor 和 SoA traversal workspace；memory gate 要求每个 `Scene` 的 live DrawCmd buffer 数量等于 `1`。

H747 Player 的 `player_md3_memory_evidence.txt` 将这些值写入
`[vivid_static_memory_profile]`。memory gate 要求字段完整，并再次验证：

```text
total_bytes <= upper_bound_bytes
total_bytes + min_headroom_bytes <= budget_bytes
exact_headroom_bytes == budget_bytes - total_bytes
exact_headroom_bytes >= min_headroom_bytes
```

## 栈准入

`PRODUCT` 与 `MCU_MIN` 使用 GNU 或 Clang 的 `-fstack-usage` 生成 `.su` 证据。目标构建完成后，Vivid 栈门只检查当前 featureset 选中的 Scene、SoA render/layout/input/semantic 与 DrawCmd compaction/execution 热路径 module：

```text
${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/stack_usage_manifest.txt
```

任一 Vivid 函数超过 `CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES`，或出现 unbounded dynamic stack usage，构建直接失败并报告函数名、实际字节数和 `vivid-stack-usage` 规则名。workspace 由对象持有并在单 UI 执行域串行复用，因此同一 `Scene` / `SoaGui` / `SoaKernel` 上的渲染、布局、输入和 semantic 操作不可重入。

## 验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/vivid_static_memory_admission_smoke.ps1
```

脚本始终复用 `Examples/ui/vivid/soa_demo/cmake-build-soa-ci`，依次验证 FULL profile-only、MCU_MIN/PRODUCT 正例、缺预算与预算不足负例，最后恢复 FULL 配置。
