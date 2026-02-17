<div align="center">

# ✨ Charm ✨

**C++26 Modules · Zero-alloc · constexpr config · Type-level FSM**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)

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
- `Draft/Examples/` —— 历史示例与杂项（已统一迁移）
- `Draft/` —— 计划、规范与架构草案

## 🚀 主线 Demos（Windows）
- **M0** `Draft/Examples/windows/main.cpp` ：kernel + timer + event queue
- **M1** `Draft/Examples/windows/main_m1.cpp` ：sync + IPC
- **M2** `Draft/Examples/windows/main_m2.cpp` ：thread + blocking
- **M3** `Draft/Examples/windows/main_m3.cpp` ：trace + stats
> 实验性 demo 故意不进主线，保持核心纯净。

### ⚡ 快速构建（Windows）
```bash
cmake -S Draft/Examples/windows -B Draft/Examples/windows/build -G Ninja
cmake --build Draft/Examples/windows/build
Draft/Examples/windows/build/os-example-win.exe
```

## 🧩 可选模块（默认关闭）
- 动态注册：`kernel.dynamic_registry` / `kernel.task_pool` / `kernel.task_auto`
- 动态优先队列：`kernel.event_queue_list`
- 可观测性：`kernel.trace`、alert/replay、JSON 诊断
- 事件策略：dedup / debounce / coalesce / boost，丢弃策略

## 🛰 MCU Demo
- 位置：`Draft/Examples/stm32f103c8`
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
- Windows：`Draft/Examples/windows/port.kernel.windows.cpp`
- STM32：`Draft/Examples/stm32f103c8/Core/Src/kernel.port.stm32.cpp`

## 🧰 VSF 迁移模块
- HAL：`Modules/io/hal/*`
- Service：`Modules/core/service/*`
- Shell：`Modules/io/shell/*`
- Module/XIP：`Modules/system/modulex/*`
- FS：`Modules/io/fs/*`

独立示例（已迁移至 Draft/Examples）：
- `Draft/Examples/hal_demo`：HAL 接口最小示例
- `Draft/Examples/vsf_service_shell`：Service + Shell + Module 示例
- `Draft/Examples/vsf_service_core`：Service 基础示例
- `Draft/Examples/vsf_fs_demo`：VFS + RAMFS 试验

### 示例构建
```bash
# HAL demo
cmake -S Draft/Examples/hal_demo -B Draft/Examples/hal_demo/build -G Ninja
cmake --build Draft/Examples/hal_demo/build
Draft/Examples/hal_demo/build/hal-demo

# Service/Shell/Module demo
cmake -S Draft/Examples/vsf_service_shell -B Draft/Examples/vsf_service_shell/build -G Ninja
cmake --build Draft/Examples/vsf_service_shell/build
Draft/Examples/vsf_service_shell/build/vsf-service-shell-demo

# Service core demo
cmake -S Draft/Examples/vsf_service_core -B Draft/Examples/vsf_service_core/build -G Ninja
cmake --build Draft/Examples/vsf_service_core/build
Draft/Examples/vsf_service_core/build/vsf-service-core-demo

# FS demo
cmake -S Draft/Examples/vsf_fs_demo -B Draft/Examples/vsf_fs_demo/build -G Ninja
cmake --build Draft/Examples/vsf_fs_demo/build
Draft/Examples/vsf_fs_demo/build/vsf-fs-demo

# Alg demo
cmake -S Draft/Examples/alg_demo -B Draft/Examples/alg_demo/build -G Ninja
cmake --build Draft/Examples/alg_demo/build
Draft/Examples/alg_demo/build/alg-demo

# Service DS demo
cmake -S Draft/Examples/service_ds_demo -B Draft/Examples/service_ds_demo/build -G Ninja
cmake --build Draft/Examples/service_ds_demo/build
Draft/Examples/service_ds_demo/build/service-ds-demo

# Bootloader demo
cmake -S Draft/Examples/bootloader_demo -B Draft/Examples/bootloader_demo/build -G Ninja
cmake --build Draft/Examples/bootloader_demo/build
Draft/Examples/bootloader_demo/build/bootloader-demo

# SDL3 WAV demo
cmake -S Draft/Examples/sdl3_wav_demo -B Draft/Examples/sdl3_wav_demo/build -G Ninja
cmake --build Draft/Examples/sdl3_wav_demo/build
Draft/Examples/sdl3_wav_demo/build/sdl3-wav-demo <file.wav>
```

## ✅ 收敛状态
- Windows 主线 M0–M3：已通过
- HAL demo：已通过（[hal_demo] ok）
- Service/Shell/Module demo：已通过（[shell] / [shell_time] / [module_demo]）
- Service core demo：已通过（[distbus] / [service_core] ok）
- FS demo：待验证
- STM32：编译通过（待烧录验证）

## 📚 文档（Draft）
- 主线：Draft/main_route_alignment.md, Draft/main_route_plan.md
- M1/M2/M3：Draft/m1_sync_spec.md, Draft/m1_tests.md, Draft/m2_thread_spec.md, Draft/m2_api_freeze.md, Draft/m3_observability_plan.md
- HAL/生态：Draft/charm_hal_design.md, Draft/charm_ecosystem_layers.md, Draft/vsf_feature_mapping.md
- 迁移范围：Draft/migration_scope_prune.md
- 迁移总览：Draft/vsf_migration_index.md
- HAL 绑定：Draft/hal_platform_binding_guide.md
- Service 细化：Draft/vsf_migration_service_detail.md
- FS 迁移：Draft/fs_migration_notes.md
- 收敛总结：Draft/convergence_summary.md

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
