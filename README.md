<div align="center">

# ✨ Charm ✨

**C++26 Modules · Zero-alloc · constexpr config · Type-level FSM**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
<br>
[![CLang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![CLang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

> 统一的模块化架构拼图：核心/系统/IO/媒体/UI 一体化组织，随取随插。

</div>

---

## 🌌 为什么是 Charm？
- **模块化极致**：全程 C++ Modules，边界清晰，可组合、可裁剪。
- **零堆内存**：std::array/std::span + 编译期规划，嵌入式友好。
- **类型驱动**：constexpr/consteval + concepts，配置即校验，能力即约束。
- **可选增强**：事件去重/防抖/合并、优先级提升、trace/alert/stats，按需点亮。

## 🗂 目录导览
- `Modules/core/` —— util/trace/service/alg
- `Modules/system/` —— kernel/modulex/boot
- `Modules/io/` —— hal/port/fs/shell/out
- `Modules/media/` —— audio
- `Modules/ui/ink/` —— Charm-ink UI
- `Modules/ui/vivid/` —— Charm-vivid UI
- `Modules/platform/` —— 平台适配（win/未来 MCU）
- `Modules/io/usb/` —— USB 设备端骨架与类草案
- `Examples/` —— 示例工程（内核/boot/audio/fs/shell/service/alg/hal）
- `docs/` —— 架构与协作文档
- `Draft/` —— 计划/草案（可变动）

## 🎯 参与入口（按方向快速上手）

### Audio
1. 读文档：`Modules/media/audio/audio_design.md`
2. 看实现：`Modules/media/audio/`（player/sink/decoder/fifo/src/convert）
3. 跑示例：`Examples/audio/sdl3_wav_demo`

### Kernel
1. 读文档：`Modules/system/kernel/docs/`
2. 看实现：`Modules/system/kernel/`
3. 跑示例：`Examples/kernel/windows`

### FS/VFS
1. 读文档：`Modules/io/fs/fs_migration_notes.md`
2. 看实现：`Modules/io/fs/`
3. 跑示例：`Examples/fs/`

### HAL/Port
1. 读文档：`Modules/io/hal/charm_hal_design.md`、`Modules/io/hal/hal_platform_binding_guide.md`
2. 看实现：`Modules/io/hal/`、`Modules/io/port/`
3. 跑示例：`Examples/hal/hal_demo`

### Shell/Service
1. 读文档：`Modules/io/shell/vsf_migration_service_shell_module.md`、`Modules/core/service/vsf_migration_service_detail.md`
2. 看实现：`Modules/io/shell/`、`Modules/core/service/`
3. 跑示例：`Examples/shell/`、`Examples/service/`

### ModuleX
1. 读文档：`Modules/system/modulex/ModuleX_格式草案.md`
2. 看实现：`Modules/system/modulex/`
3. 跑示例：`Examples/shell/vsf_shell_fs_module`

### USB
1. 读文档：`docs/usb_arch_plan.md`
2. 看实现：`Modules/io/usb/`
3. 跑示例：`Examples/usb/usb_cdc_minimal`

### UI/Vivid
1. 读文档：`Modules/ui/vivid/ARCHITECTURE.md`、`Modules/ui/vivid/FEATURES.md`
2. 看实现：`Modules/ui/vivid/`
3. 跑示例：`Examples/project/scope`

## 🚀 主线 Demos（Windows）
- **M0** `Examples/kernel/windows/main.cpp` ：kernel + timer + event queue
- **M1** `Examples/kernel/windows/main_m1.cpp` ：sync + IPC
- **M2** `Examples/kernel/windows/main_m2.cpp` ：thread + blocking
- **M3** `Examples/kernel/windows/main_m3.cpp` ：trace + stats
> 实验性 demo 故意不进主线，保持核心纯净。

### ⚡ 快速构建（Windows）
```bash
cmake -S Examples/kernel/windows -B Examples/kernel/windows/build -G Ninja
cmake --build Examples/kernel/windows/build
Examples/kernel/windows/build/os-example-win.exe
```

## 🧩 可选模块（默认关闭）
- 动态注册：`kernel.dynamic_registry` / `kernel.task_pool` / `kernel.task_auto`
- 动态优先队列：`kernel.event_queue_list`
- 可观测性：`kernel.trace`、alert/replay、JSON 诊断
- 事件策略：dedup / debounce / coalesce / boost，丢弃策略

## 🛰 MCU Demo
- 位置：`Draft/Examples/stm32f103c8`（待迁移）
- 开关：`-DCHARM_MCU_KERNEL_DEMO=ON`（preset 默认开启）
- 入口：`main_mcu_stub.cpp`（`application()` -> `run_auto`）
- 平台绑定：`Core/Src/kernel.port.stm32.cpp`

### 🔧 MCU 构建示例
```bash
cmake --preset Release -S Draft/Examples/stm32f103c8 -B Draft/Examples/stm32f103c8/build
cmake --build Draft/Examples/stm32f103c8/build --target vivid-example-stm32
```
> 烧录按板级工具链流程执行。

## 🌉 Port 层（平台无关）
- 接口：`Modules/io/port/port.kernel.cppm`
- 模板：`Modules/io/port/port.kernel.template.cpp`
- Windows：`Examples/kernel/windows/port.kernel.windows.cpp`
- STM32：`Draft/Examples/stm32f103c8/Core/Src/kernel.port.stm32.cpp`

## 🧰 VSF 迁移模块
- HAL：`Modules/io/hal/*`
- Service：`Modules/core/service/*`
- Shell：`Modules/io/shell/*`
- Module/XIP：`Modules/system/modulex/*`
- FS：`Modules/io/fs/*`

独立示例（Examples）：
- `Examples/hal/hal_demo`：HAL 接口最小示例
- `Examples/shell/vsf_service_shell`：Service + Shell + Module 示例
- `Examples/service/vsf_service_core`：Service 基础示例
- `Examples/service/service_ds_demo`：Service DS 示例
- `Examples/fs/vsf_fs_demo`：VFS + RAMFS 试验
- `Examples/fs/vsf_fs_block_demo`：BlockFS 试验
- `Examples/fs/vsf_fs_vfs_demo`：VFS 组合示例
- `Examples/fs/vsf_fs_posix`：POSIX 封装示例
- `Examples/shell/vsf_shell_fs_module`：Shell + FS + ModuleX 示例
- `Examples/alg/alg_demo`：算法/压缩示例
- `Examples/boot/bootloader_demo`：bootloader 示例
- `Examples/audio/sdl3_wav_demo`：SDL3 音频示例
- `Examples/usb/usb_cdc_minimal`：CDC 最小枚举示例

### 示例构建
```bash
# HAL demo
cmake -S Examples/hal/hal_demo -B Examples/hal/hal_demo/build -G Ninja
cmake --build Examples/hal/hal_demo/build
Examples/hal/hal_demo/build/hal-demo

# Service/Shell/Module demo
cmake -S Examples/shell/vsf_service_shell -B Examples/shell/vsf_service_shell/build -G Ninja
cmake --build Examples/shell/vsf_service_shell/build
Examples/shell/vsf_service_shell/build/vsf-service-shell-demo

# Service core demo
cmake -S Examples/service/vsf_service_core -B Examples/service/vsf_service_core/build -G Ninja
cmake --build Examples/service/vsf_service_core/build
Examples/service/vsf_service_core/build/vsf-service-core-demo

# FS demo
cmake -S Examples/fs/vsf_fs_demo -B Examples/fs/vsf_fs_demo/build -G Ninja
cmake --build Examples/fs/vsf_fs_demo/build
Examples/fs/vsf_fs_demo/build/vsf-fs-demo

# Alg demo
cmake -S Examples/alg/alg_demo -B Examples/alg/alg_demo/build -G Ninja
cmake --build Examples/alg/alg_demo/build
Examples/alg/alg_demo/build/alg-demo

# Service DS demo
cmake -S Examples/service/service_ds_demo -B Examples/service/service_ds_demo/build -G Ninja
cmake --build Examples/service/service_ds_demo/build
Examples/service/service_ds_demo/build/service-ds-demo

# Bootloader demo
cmake -S Examples/boot/bootloader_demo -B Examples/boot/bootloader_demo/build -G Ninja
cmake --build Examples/boot/bootloader_demo/build
Examples/boot/bootloader_demo/build/bootloader-demo

# SDL3 WAV demo
cmake -S Examples/audio/sdl3_wav_demo -B Examples/audio/sdl3_wav_demo/build -G Ninja
cmake --build Examples/audio/sdl3_wav_demo/build
Examples/audio/sdl3_wav_demo/build/sdl3-wav-demo <file.wav>
```

## ✅ 收敛状态
- Windows 主线 M0–M3：已通过
- HAL demo：已通过（[hal_demo] ok）
- Service/Shell/Module demo：已通过（[shell] / [shell_time] / [module_demo]）
- Service core demo：已通过（[distbus] / [service_core] ok）
- FS demo：已构建（待运行验证）
- STM32：编译通过（待烧录验证）

## 📚 文档
- 架构总览：`docs/architecture_overview.md`
- 输入分层：`docs/input_layering_decision.md`
- 协作规范：`docs/《协作期待与规范》.md`
- 协作认知：`docs/《现代 C++ 单片机代码协作认知》.md`
- 推进与分工：`docs/推进TODO与分工.md`、`docs/refactor_todo_ownership.md`
- 组件文档：Audio=`Modules/media/audio/audio_design.md`、HAL=`Modules/io/hal/charm_hal_design.md`、FS=`Modules/io/fs/fs_migration_notes.md`、Shell=`Modules/io/shell/vsf_migration_service_shell_module.md`、Service=`Modules/core/service/vsf_migration_service_detail.md`、ModuleX=`Modules/system/modulex/ModuleX_格式草案.md`、Kernel=`Modules/system/kernel/docs/`

## 🧰 新示例模板（CMake）

快速创建新示例工程：
- 模板：`Examples/cmake/ExampleTemplate.cmake`
- 推荐用法：在新示例 `CMakeLists.txt` 中 `include(...)`，然后调用 `charm_example_*` 系列函数
- SDL3 统一入口：`cmake/SDL3.cmake`（优先 `find_package`，回退到 `Examples/ThirdParty/SDL3`）

## 🗺 路线图（摘要）
- **近期**：MCU 运行验证、HAL MVP（GPIO/UART/Timer）、shim 最小 POSIX time/sleep
- **中期**：module loader/XIP 草案、RT facade（线程 API 包装）、VFS 完善（目录/多块）
- **远期**：POSIX/Arduino 更丰富、模块热插拔/签名、USB/TCPIP/FS 组件按需引入、FatFS/FlashFS 适配

## ✅ 未来 TODO（主线闭环）
- **升级闭环**：固件签名/密钥管理、版本治理、回滚策略、A/B 分区策略、OTA/下载通道
- **平台闭环**：统一驱动模型、BSP 组织结构、板级模板、移植指南、最小可跑清单
- **运行闭环**：监控/诊断/异常恢复、崩溃转储、健康度指标与故障归因
- **存储闭环**：VFS 挂载策略、缓存/一致性、轻量日志或事务
- **通信闭环**：统一 IO/transport 抽象、RPC/消息协议、调试通道
- **安全闭环**：只读区验证、最小信任链、密钥封装接口
- **工程化**：脚手架/模板工程、依赖图与生成文档、PC/MCU smoke 测试

## 📜 许可
MIT（见 LICENSE）。
