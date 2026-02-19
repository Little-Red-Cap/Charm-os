# FatFs 文件镜像示例（PC 端验证）

目标：用 **file-backed block device + FatFs** 打通 “块设备 -> FAT32 -> VFS -> 上层” 链路。

## 前置条件

- FatFs 源码放在 `Modules/thirdparty/fatfs/`
  - 必含：`ff.c`、`ff.h`、`diskio.h`、`ffconf.h`、`integer.h`
- CMake 开关：`-DCHARM_ENABLE_FATFS=ON`
- 镜像文件（例如 `dev.vhd` 或 `.img`），已初始化为 MBR + FAT32

建议配置（`ffconf.h`）：
- `FF_USE_LFN = 2` 或 `3`（启用 LFN）
- `FF_MAX_LFN` 按需设置

可选工程开关（构建时）：
- `CHARM_FATFS_MAX_FILES`（最大打开文件数）
- `CHARM_FATFS_MAX_PATH`（路径长度上限）

## 示例工程

路径：`Examples/fs/vsf_fs_fatfs_demo`

行为：
- 读取镜像 `LBA0`，解析 MBR 分区表，定位 FAT32 分区
- 挂载 FatFs 到 `/`
- 列出根目录并读取 `/hello.txt`

## 使用方式（示例）

```bash
# 配置
cmake -S . -B cmake-build-debug -DCHARM_ENABLE_FATFS=ON -DCHARM_USE_ETL=OFF

# 构建
cmake --build cmake-build-debug -j 12

# 运行（传入镜像路径）
Examples/fs/vsf_fs_fatfs_demo/<build>/vsf-fs-fatfs-demo G:\Project\dev.vhd
```

## 说明与注意事项

- 默认 block size 为 512。
- `vfs_open(path)` 默认只读；创建/截断需使用 `OpenFlags`。
- LFN 需要 `FF_USE_LFN` 开启且 `FILINFO.lfname/lfsize` 已传入。
- `BlockFile` 已改为 64-bit seek，可支持 2GB 以上镜像。
