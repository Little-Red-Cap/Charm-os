# USB String/Lang Descriptor

## 文档状态

- `status`: `supporting`
- `scope`: compile-time string descriptor 与 `DescriptorTable` 索引
- `source`: [`usb.common.cppm`](../../Modules/io/usb/common/usb.common.cppm) 与
  [`usb.device.cppm`](../../Modules/io/usb/device/usb.device.cppm)

## 构造与索引

- `make_lang_id_descriptor(const u16 (&)[N])` 接收非空 LangID 数组；N 上限为 126。
- `make_ascii_string_descriptor()` 将每个 char 直接写为 UTF-16LE 低字节，不解析 UTF-8。
- 非 ASCII 文本使用 `make_utf16_string_descriptor(const char16_t (&)[N])`。
- string helper 最多接受 127 个 code unit，超限在编译期失败。

`DescriptorTable::strings[index]` 直接对应 USB string descriptor index。Index 0 应放 LangID；device、
configuration 和 class descriptor 中的 string index 必须与表位置一致。

## 生命周期与限制

- `DescriptorTable` 只保存 span array 指针和 count，不复制 descriptor bytes；table、span array 和底层
  byte array 必须在 provider 使用期间有效。
- 越界 index、空 span 或缺失 table 返回 descriptor lookup 失败。
- 默认 table provider 只接收 descriptor type/index，不按 setup `wIndex` 选择语言；当前没有同一
  string index 的多语言变体路由。
- helper 只生成 byte layout，不验证产品 string policy、host 显示结果或设备枚举。
