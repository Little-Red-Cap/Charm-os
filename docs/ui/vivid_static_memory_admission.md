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

`MCU_MIN` 必须显式设置：

- `CHARM_VIVID_RUNTIME_SCENE_INSTANCES`
- `CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES`
- `CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES`

`PRODUCT` 不直接设置这些缓存变量，而是通过两层具名 DSL 提供同一信息：

- `vivid_define_product_profile()`：SoA nodes、TextArena、Style、DrawCmd、float widget、active kinds 与 payload capacities；
- `vivid_configure_product_target()`：Scene 数、屏幕/像素格式、layer cache、RAM budget/headroom 与热函数栈上限。

一个 `Charm-ui` target 只能选择一个 profile。PRODUCT 下以下旧变量会触发迁移错误，不存在兼容双轨：

- `CHARM_VIVID_PRODUCT_CORE_MODULES`
- `CHARM_VIVID_PRODUCT_GFX_MODULES`
- `CHARM_VIVID_PRODUCT_WIDGETS`
- `CHARM_VIVID_PAYLOAD_CAP_*`

FULL/MCU_MIN 的 DrawCmd 与热路径栈画像仍由以下配置控制；PRODUCT 中对应值来自 profile 与 target envelope：

- `CHARM_VIVID_DRAW_CMD_MAX_COMMANDS`
- `CHARM_VIVID_DRAW_CMD_TEXT_BYTES`
- `CHARM_VIVID_DRAW_CMD_BLOB_BYTES`
- `CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES`

默认 DrawCmd profile 为 `1024/4096/2048`，默认 Vivid 栈帧上限为 `4096B`。PRODUCT profile 必须显式固定这些值；容量下调必须有真实产品峰值证据。

PRODUCT/MCU_MIN 的 Scene 实例数、预算和最小余量都不能依赖隐式默认值，且必须大于零。

`FULL` 生成画像但默认不执行预算准入。所有 featureset 的配置证据写入当前 target/profile 隔离目录：

```text
${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/<target>/<profile>/
```

例如 `soa_demo` 将 Vivid 作为 `Charm` 子项目引入时，FULL 画像位于
`<build>/Charm/generated/vivid/Charm-ui/full/static_memory_admission.txt`；Player PRODUCT base profile 位于
`<build>/Charm/generated/vivid/Charm-ui/player_md3/`。

PRODUCT 目录额外生成：

- `profile.json`：catalog、active kinds、payload capacities、产品工作集与 `profile_fingerprint`；
- `target_envelope.json`：硬件 envelope 与 `target_fingerprint`；
- `module_closure.json`：当前 Vivid modules/sources 与 external requirements；
- `admission.json`：结构化静态内存准入结果；
- typed config、pool caps、active feature 代码和 stack source manifest。

`profile_fingerprint` 不包含屏幕和 RAM envelope，同一产品 profile 的 Host/MCU 消费者必须一致；`target_fingerprint` 包含 envelope，目标差异必须可见。

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

`PRODUCT` 与 `MCU_MIN` 使用 GNU 或 Clang 的 `-fstack-usage` 生成 `.su` 证据。目标构建完成后，Vivid 栈门检查当前 featureset 实际选中的全部 Vivid module：

```text
${CMAKE_CURRENT_BINARY_DIR}/generated/vivid/<target>/<profile>/stack_usage_manifest.txt
```

任一选中 module 内的 Vivid 函数超过 `CHARM_VIVID_MAX_HOT_STACK_FRAME_BYTES`，或出现 unbounded dynamic stack usage，构建直接失败并报告函数名、实际字节数和 `vivid-stack-usage` 规则名。manifest 以当前选中 source 清单过滤 `.su`，因此复用构建目录切换 featureset 不会引入旧 module 证据。

该门证明的是单函数 frame 上限，不是累计调用链峰值。对象内 workspace 由单 UI 执行域串行复用；SVG raster workspace 则由调用方显式持有并传入。二者都不支持同一 workspace 上的并发或重入调用，产品任务栈仍需由实际入口的调用链或运行时 high-water 证据补齐。

## 验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/vivid_product_profile_compiler_smoke.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/vivid_static_memory_admission_smoke.ps1
```

两条脚本始终复用 `Examples/ui/vivid/soa_demo/cmake-build-soa-ci`：

- compiler smoke 以真实 `player_md3` profile 对比 Host/H747 envelope fingerprint，但不配置或构建 H747 固件；同时覆盖 duplicate kind/ID/module/profile、未知/internal/host-only root、closure cycle、payload、继承、旧变量与同 target 多 profile 负例；
- static-memory smoke 依次验证 FULL profile-only、MCU_MIN 正负例、Player PRODUCT base/debug/base 切换、预算不足和 closure/stack source 无污染，最后恢复 FULL 配置。
