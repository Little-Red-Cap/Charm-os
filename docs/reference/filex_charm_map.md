# FileX 与 Charm storage 对照

> `status`: `reference`
>
> 本文用于比较 FileX 与 Charm block/MAL/VFS 形状，不描述当前 Charm 契约。

## 对象映射

| FileX | Charm 对照 | 状态 |
|---|---|---|
| `FX_MEDIA` | `MalDevice` + `FatFsMount` | Charm 分离 media view 与 mount state |
| `fx_media_driver_entry` | `MalOps` | 当前只有 ops table，没有 request-code 单入口 |
| `fx_media_driver_info` | `MalDevice::ctx` | 均为 backend context |
| `fx_media_open/close` | `FatFsMount::mount/unmount` | 当前实现存在 |
| `fx_media_format` | `mount(..., format_if_needed)` | 受 FatFs build option 约束 |
| `FX_FILE` / `fx_file_*` | `fs::File` / `MountOps` | 由 VFS/FatFs adapter 提供 |
| Unicode API | FatFs UTF-8/UTF-16/OEM conversion | 当前位于 adapter 内部 |
| media statistics | trace/fs statistics | 未形成统一接口 |
| fault tolerant journal | 无直接对应 | 未实现 |

## 可借鉴点

- backend context 显式传递，不使用隐式全局 driver object；
- cache memory 由 caller 提供，容量和 footprint 可见；
- mount 隐藏 filesystem 内部状态；
- Unicode 转换边界明确；
- statistics 和 power-fail recovery 作为可选能力，不污染最小 IO surface。

## 不应误读

- `MalDriverEntry` request-code API 只是早期提案，当前源码不存在。
- FileX 的 control block、format、statistics 和 fault-tolerant log 不会因本映射自动进入 Charm。
- `MalDevice::ctx` 相似不表示 FileX driver ABI 与 `MalOps` 兼容。
- 参考实现的宏配置不能直接定义 Charm 的 build/runtime policy。

当前实现入口为 [`storage/README.md`](../storage/README.md) 与 `Modules/io/fs/fs_mal*.cppm`。
