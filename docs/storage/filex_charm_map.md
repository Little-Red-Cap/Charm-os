# FileX → Charm 对标表（BlockDevice / MAL / VFS）

目标：把 FileX 的结构与 Charm 当前 FS/MAL/VFS 体系对齐，提炼可复用的设计点。

## 1. 核心对象映射

| FileX 概念 | FileX 对象 | Charm 对标 | 备注 |
| --- | --- | --- | --- |
| 介质控制块 | `FX_MEDIA` | `MalDevice` + `FatFsMount` | FileX 把缓存/状态集中在 `FX_MEDIA`；Charm 可由 Mount 持有。 |
| 驱动入口 | `fx_media_driver_entry` | `MalOps` / `MalDriverEntry` | 建议保留“单入口驱动”规范，利于统一设备栈。 |
| 驱动上下文 | `fx_media_driver_info` | `MalDevice::ctx` | 设备私有上下文指针。 |
| 媒体打开/关闭 | `fx_media_open/close` | `FatFsMount::mount/unmount` | Charm 当前通过 Mount 管理。 |
| 媒体格式化 | `fx_media_format` | FatFs format（规划中） | 可作为未来 API 预留。 |
| 文件句柄 | `FX_FILE` | `fs::File` / VFS File | 对应 VFS 统一文件抽象。 |
| 文件 API | `fx_file_*` | `MountOps::{open,read,write,seek,close}` | 语义对齐，重点在 open 模式。 |
| Unicode API | `fx_unicode_*` | LFN UTF‑16↔UTF‑8 | Charm 目前在 FatFs 适配层处理。 |
| 统计/诊断 | `FX_MEDIA` 统计字段 | `trace_core` / fs 统计 | 可做可选统计层。 |
| 容错日志 | `fx_fault_tolerant` | （可选）FS 事务日志 | 适合作为高级选项。 |

## 2. 驱动接口形状对标

FileX 驱动是一条入口+请求码：

- 请求码：`READ/WRITE/FLUSH/INIT/BOOT_READ/UNINIT/...`
- 上下文字段：`driver_info`
- 统一入口：`driver_entry(media_ptr)`

Charm 对标建议：

- 保持 `MalOps` 稳定 API
- 提供可选的 `MalDriverEntry` 规范，让底层驱动按“请求码”实现
- 用 `MalDevice::ctx` 承载 driver_info

这样可兼容：
- 统一驱动（RAM/Flash/USB/远端）
- 可移植到 MCU/PC 两端

## 3. 缓存/内存管理对标

FileX：
- cache buffer 由应用在 `fx_media_open` 传入
- 缓存大小决定可缓存扇区数（非内部 malloc）

Charm 建议：
- 在 `FatFsMount` 增加“可选 cache buffer”输入
- 以 Mount 为单位配置缓存，不侵入 `MalDevice`

## 4. 关键可借鉴点（建议落地）

1) 单入口驱动规范（见 `docs/storage/mal_overview.md`）
2) 显式 driver_info / ctx 语义
3) 应用提供缓存内存（可控 footprint）
4) 统一统计接口（可选）
5) Unicode API 明确化（显式 UTF‑8/UTF‑16 转换）

## 5. 需要避免的点

- 过多宏驱动配置（可改为 CMake + static_assert）
- 把 FAT 内部状态暴露到上层（保持 Mount 封装）

## 6. 下一步落地建议

- 在 `MAL + FatFs` 示例中补充“driver_entry 规范”说明
- 给 `FatFsMount` 预留 cache buffer 参数（可选）
- 把 FileX 的统计字段映射为 `trace_core` 事件或统计结构

