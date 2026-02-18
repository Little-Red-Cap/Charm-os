# Examples 总览

此目录收纳可复现的示例工程，均可独立配置与编译（不运行）。

## 目录结构

- `kernel/windows`：内核 M0–M3 主线
- `boot/bootloader_demo`：bootloader 示例
- `audio/sdl3_wav_demo`：SDL3 音频示例
- `fs/`：VFS/BlockFS/POSIX 示例
- `shell/`：Shell/ModuleX 组合示例
- `service/`：Service 核心与 DS 示例
- `hal/hal_demo`：HAL 接口示例
- `alg/alg_demo`：算法与压缩示例

## 快速构建（Windows + Ninja）

```bash
# Kernel M0–M3
cmake -S Examples/kernel/windows -B Examples/kernel/windows/build -G Ninja
cmake --build Examples/kernel/windows/build

# Bootloader
cmake -S Examples/boot/bootloader_demo -B Examples/boot/bootloader_demo/build -G Ninja
cmake --build Examples/boot/bootloader_demo/build

# Audio (SDL3)
cmake -S Examples/audio/sdl3_wav_demo -B Examples/audio/sdl3_wav_demo/build -G Ninja
cmake --build Examples/audio/sdl3_wav_demo/build

# FS (VFS / BlockFS / POSIX)
cmake -S Examples/fs/vsf_fs_demo -B Examples/fs/vsf_fs_demo/build -G Ninja
cmake --build Examples/fs/vsf_fs_demo/build

cmake -S Examples/fs/vsf_fs_block_demo -B Examples/fs/vsf_fs_block_demo/build -G Ninja
cmake --build Examples/fs/vsf_fs_block_demo/build

cmake -S Examples/fs/vsf_fs_vfs_demo -B Examples/fs/vsf_fs_vfs_demo/build -G Ninja
cmake --build Examples/fs/vsf_fs_vfs_demo/build

cmake -S Examples/fs/vsf_fs_posix -B Examples/fs/vsf_fs_posix/build -G Ninja
cmake --build Examples/fs/vsf_fs_posix/build

# Shell + ModuleX
cmake -S Examples/shell/vsf_service_shell -B Examples/shell/vsf_service_shell/build -G Ninja
cmake --build Examples/shell/vsf_service_shell/build

cmake -S Examples/shell/vsf_shell_fs_module -B Examples/shell/vsf_shell_fs_module/build -G Ninja
cmake --build Examples/shell/vsf_shell_fs_module/build

# Service
cmake -S Examples/service/vsf_service_core -B Examples/service/vsf_service_core/build -G Ninja
cmake --build Examples/service/vsf_service_core/build

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

- 示例默认关闭 UI/Media/SDL3 依赖（仅在对应示例中启用）。
- 运行方式与输出说明请参考根目录 `README.md`。

## SDL3 安装与配置（PC 验证用）

优先使用系统安装的 SDL3（CMake `find_package(SDL3 CONFIG)`）。

### 方式 A：本机安装（推荐）

1. 从 SDL3 源码编译并安装：
   ```bash
   cmake -S <SDL3_SOURCE> -B <SDL3_BUILD> -G Ninja -DCMAKE_INSTALL_PREFIX=<SDL3_PREFIX>
   cmake --build <SDL3_BUILD>
   cmake --install <SDL3_BUILD>
   ```
2. 设置环境变量（或 CMake cache）：
   - `SDL3_DIR=<SDL3_PREFIX>/lib/cmake/SDL3`

### 方式 B：项目内源码

把 SDL3 源码放到：

```
Examples/ThirdParty/SDL3
```

示例会自动优先 `find_package`，找不到时回退到本地源码目录。
