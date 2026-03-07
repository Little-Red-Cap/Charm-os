# USB String/Lang Descriptor 装配示例

本页给出最小 string/lang 装配示例，用于填充 `DescriptorTable::strings`。

## 最小示例

```cpp
using namespace usb;

static constexpr auto lang_desc = make_lang_id_descriptor({ 0x0409 });
static constexpr auto mfg_desc  = make_ascii_string_descriptor("Charm");
static constexpr auto prod_desc = make_ascii_string_descriptor("Charm Device");
static constexpr auto ser_desc  = make_ascii_string_descriptor("0001");

static constexpr std::span<const u8> strings[] = {
    std::span<const u8>(lang_desc.data(), lang_desc.size()),
    std::span<const u8>(mfg_desc.data(), mfg_desc.size()),
    std::span<const u8>(prod_desc.data(), prod_desc.size()),
    std::span<const u8>(ser_desc.data(), ser_desc.size()),
};

DescriptorTable table{};
table.strings = strings;
table.string_count = std::size(strings);
```

## 约束

- index 0 必须是 LangID 描述符。
- 其余 string index 与 `DeviceDescriptor::manufacturer/product/serial_number` 对应。
- ASCII 字符串使用 `make_ascii_string_descriptor`，UTF-16 使用 `make_utf16_string_descriptor`。
