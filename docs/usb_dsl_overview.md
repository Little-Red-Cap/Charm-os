# USB DSL overview (DescriptorTable + ConfigTree)

This note documents the minimal DSL skeleton and a concrete assembly example
for DescriptorTable and ConfigTree.

## DSL scope

The DSL is a thin, strongly-typed builder that emits raw descriptor bytes:

- Device/Config/Interface/Endpoint descriptors are assembled with `ConfigBuilder`.
- Class descriptors are passed as raw byte spans (validated by length fields).
- Output is compatible with `device::DescriptorTable` + `device::ConfigTree`.

## Minimal assembly example (CDC ACM)

```cpp
using namespace usb;

std::array<u8, sizeof(DeviceDescriptor)> dev_desc{};
std::array<u8, 256> cfg_desc{};
device::DescriptorTable table{};
device::ConfigTree tree{};

usb::dsl::DeviceBuildContext ctx{
    std::span<u8>(dev_desc.data(), dev_desc.size()),
    std::span<u8>(cfg_desc.data(), cfg_desc.size()),
    &table,
    &tree,
};

usb::dsl::DeviceInfo dev_info{};
dev_info.vendor_id = 0xCafe;
dev_info.product_id = 0x4000;
dev_info.i_manufacturer = 1;
dev_info.i_product = 2;
dev_info.i_serial = 3;

usb::dsl::ConfigInfo cfg_info{};
cfg_info.configuration_value = 1;
cfg_info.max_power = 50;

class_driver::CdcConfig cdc_cfg{};
cdc_cfg.ctrl_ifc = 0;
cdc_cfg.data_ifc = 1;
cdc_cfg.ep_notify = 0x81;
cdc_cfg.ep_out = 0x02;
cdc_cfg.ep_in = 0x83;
cdc_cfg.ep_mps = 64;

auto cdc_desc = usb::dsl::make_cdc_acm_class_descriptors(cdc_cfg);
bool ok = usb::dsl::build_cdc_acm_device(
    ctx,
    dev_info,
    cfg_info,
    cdc_cfg,
    cdc_desc.view(),
    nullptr,
    0);
```

## DescriptorTree/DescriptorTable usage

`build_*_device()` fills both:

- `DescriptorTable` (device/config/string descriptors)
- `ConfigTree` (config bytes + view span)

These can be plugged into `usb.device` via:

```cpp
dev.set_descriptor_provider(device::make_descriptor_provider(table));
dev.set_config_tree(tree);
```

## Validation rules

- Endpoint address `0x00` is reserved (EP0).
- Endpoint direction conflicts are rejected in `ConfigBuilder`.
- Class descriptor streams must be length-valid (`validate_descriptor_stream`).
