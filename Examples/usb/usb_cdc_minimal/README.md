# USB CDC 最小枚举示例

> status: `supporting`
>
> scope: Host-side CDC descriptor and EP0 fixture

这个示例展示 **CDC ACM 设备端最小枚举链路**：

```
DescriptorBuilder -> usb.device -> usb.ep0_driver -> DCD
```

它只关注描述符构建与 EP0 流程，不包含具体硬件驱动实现。

## 构建（Windows / Ninja）

```bash
cmake -S Examples/usb/usb_cdc_minimal -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
```

## 说明

- 描述符：在 `main.cpp` 内使用 `DescriptorBuilder` 拼接 CDC 配置树。
- 字符串：使用 `make_lang_id_descriptor` / `make_ascii_string_descriptor`。
- EP0：由 `usb.device` 处理标准请求，`usb.ep0_driver` 定义驱动契约。

该 fixture 不包含真实 DCD、bulk IN/OUT 数据通路或板级枚举，因此不能证明 CDC 设备可被主机识别或
传输数据。当前 USB 能力与验证入口见 [`docs/usb/README.md`](../../../docs/usb/README.md)。
