# USB CDC 最小枚举示例

这个示例展示了 **CDC ACM 设备端最小枚举链路**：

```
DescriptorBuilder -> usb.device -> usb.ep0_driver -> DCD
```

它只关心描述符构建与 EP0 流程，不包含具体硬件驱动实现。

## 构建（Windows / Ninja）

```bash
cmake -S Examples/usb/usb_cdc_minimal -B Examples/usb/usb_cdc_minimal/build -G Ninja
cmake --build Examples/usb/usb_cdc_minimal/build
```

## 说明

- 描述符：在 `main.cpp` 里用 `DescriptorBuilder` 拼接 CDC 设备的配置树
- 字符串：使用 `make_lang_id_descriptor` / `make_ascii_string_descriptor`
- EP0：由 `usb.device` 处理标准请求，`usb.ep0_driver` 定义驱动契约

## 后续扩展

1) 接入真实 DCD：实现 `Ep0DriverOps` 的 send_in/send_zlp/stall  
2) 补充 CDC 数据通路：bulk IN/OUT 端点收发  
3) 把枚举过程接到 Shell/Trace 做可视化验证  
