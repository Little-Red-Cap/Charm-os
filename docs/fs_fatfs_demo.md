# FatFs 文件镜像示例（PC 端验证）

目的：用 **file-backed block device + FatFs** 打通 PC 端 “块设备 → FAT32 → VFS → 上层” 链路。

## 前置条件

- FatFs 源码放在 `Modules/thirdparty/fatfs/`
  - 需包含：`ff.c`、`ff.h`、`diskio.h`、`ffconf.h`
- CMake 开关：`-DCHARM_ENABLE_FATFS=ON`
- 镜像文件（例如 `dev.vhd` 或 `.img`），且已初始化为 MBR + FAT32

## 示例工程

路径：`Examples/fs/vsf_fs_fatfs_demo`

行为：
- 读取镜像 `LBA0`，解析 MBR 分区表，自动定位 FAT32 分区
- 挂载 FatFs 到 `/`
- 列出根目录并尝试读取 `/hello.txt`

## 使用方式（示例）

```bash
# 配置
cmake -S . -B cmake-build-debug -DCHARM_ENABLE_FATFS=ON -DCHARM_USE_ETL=OFF

# 构建
cmake --build cmake-build-debug -j 12

# 运行（传入镜像路径）
Examples/fs/vsf_fs_fatfs_demo/<build>/vsf-fs-fatfs-demo G:\Project\dev.vhd
```

## 注意事项

- 默认 block size 为 512
- 若 FatFs 未启用 `FF_USE_MKFS`，示例不会自动格式化
- `vfs_close` 已接入统一回收路径，示例会调用关闭
