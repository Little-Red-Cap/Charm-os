# Examples 总览

此目录收录可复现的示例工程，主要用于验证某条能力主线、某个子系统入口或某段运行时路径。

它们默认回答的是：

- 某条能力现在最小能不能跑起来
- 某个子系统当前推荐从哪条示例进入
- 某条回归链路应该如何独立配置与构建

如果你是第一次进入仓库，建议先看根目录 [`README.md`](../README.md) 与 [`docs/README.md`](../docs/README.md)，
再回到这里按主题找示例。

## 按主题进入

- `kernel/`
  内核、RTOS、ARMv7-A、POSIX/QEMU 等运行时路径示例。
- `init/`
  bring-up / materialize / observe 相关验证示例。
- `fs/`
  VFS、BlockFS、FatFs 等存储链路示例。
- `io/`
  输入、pump、通道等 IO 相关示例。
- `service/`
  Service 核心能力与信号/状态链路示例。
- `usb/`
  USB device / host runtime 相关示例。
- `project/`
  项目化示例，当前重点是 `player/`。
- `audio/`
  SDL3 音频与主机验证路径。
- `boot/`、`hal/`、`alg/`、`system/`、`ui/`、`ink/`
  对应子系统的专项示例。

## 推荐阅读路径

- 看内核示例集合：
  [`kernel/README.md`](kernel/README.md)

- 看文件系统示例集合：
  [`fs/README.md`](fs/README.md)

- 看项目化示例集合：
  [`project/README.md`](project/README.md)

- 看项目化示例：
  [`project/player/README.md`](project/player/README.md)

- 看 ARMv7-A / QEMU bare-metal：
  [`kernel/armv7a/qemu/README.md`](kernel/armv7a/qemu/README.md)

- 看 POSIX / QEMU：
  [`kernel/posix/qemu/README.md`](kernel/posix/qemu/README.md)

- 看 USB 示例集合：
  [`usb/README.md`](usb/README.md)

- 看 FatFs / VFS 示例：
  先回到 [`../docs/storage/fs_fatfs_demo.md`](../docs/storage/fs_fatfs_demo.md)，
  再进入对应示例目录。

## 快速构建（Windows + Ninja）

```bash
# Kernel M0/M3
cmake -S Examples/kernel/windows -B Examples/kernel/windows/build -G Ninja
cmake --build Examples/kernel/windows/build

# Bootloader
cmake -S Examples/boot/bootloader_demo -B Examples/boot/bootloader_demo/build -G Ninja
cmake --build Examples/boot/bootloader_demo/build

# Audio (SDL3)
cmake -S Examples/audio/sdl3_wav_demo -B Examples/audio/sdl3_wav_demo/build -G Ninja
cmake --build Examples/audio/sdl3_wav_demo/build

# Player (Windows)
cmake -S Examples/project/player -B Examples/project/player/build -G Ninja
cmake --build Examples/project/player/build

# FS (VFS / BlockFS / FatFs)
cmake -S Examples/fs/fs_demo -B Examples/fs/fs_demo/build -G Ninja
cmake --build Examples/fs/fs_demo/build

cmake -S Examples/fs/fs_vfs_demo -B Examples/fs/fs_vfs_demo/build -G Ninja
cmake --build Examples/fs/fs_vfs_demo/build

# Shell + ModuleX
cmake -S Examples/shell/service_shell -B Examples/shell/service_shell/build -G Ninja
cmake --build Examples/shell/service_shell/build

# Service
cmake -S Examples/service/service_core -B Examples/service/service_core/build -G Ninja
cmake --build Examples/service/service_core/build

cmake -S Examples/service/service_ds_demo -B Examples/service/service_ds_demo/build -G Ninja
cmake --build Examples/service/service_ds_demo/build

# HAL
cmake -S Examples/hal/hal_demo -B Examples/hal/hal_demo/build -G Ninja
cmake --build Examples/hal/hal_demo/build

# Algorithms
cmake -S Examples/alg/alg_demo -B Examples/alg/alg_demo/build -G Ninja
cmake --build Examples/alg/alg_demo/build
```

## 说明

- 示例默认关闭 UI / Media / SDL3 等额外依赖，只有对应示例会显式开启。
- 不同示例的维护活跃度并不完全一致；优先参考子目录内带 `README.md` 的示例。
- 运行方式与输出说明，优先查看各示例目录自己的 `README.md`。

## SDL3 安装与配置（PC 验证用）

优先使用系统安装的 SDL3（CMake `find_package(SDL3 CONFIG)`）。

### 方式 A：本机安装（推荐）

1. 从 SDL3 源码编译并安装：

   ```bash
   cmake -S <SDL3_SOURCE> -B <SDL3_BUILD> -G Ninja -DCMAKE_INSTALL_PREFIX=<SDL3_PREFIX>
   cmake --build <SDL3_BUILD>
   cmake --install <SDL3_BUILD>
   ```

2. 设置环境变量或 CMake cache：

   - `SDL3_DIR=<SDL3_PREFIX>/lib/cmake/SDL3`

### 方式 B：项目内源码

把 SDL3 源码放到：

```text
Examples/ThirdParty/SDL3
```

示例会优先使用 `find_package`，找不到时回退到本地源码目录。
