# HAL ops backend 模板

## 文档状态

- `status`: `supporting`
- `scope`: 当前 `{ctx, ops}` HAL handle 的 backend 绑定
- `source`: `hal_spi.cppm`、`hal_gpio.cppm` 及同目录接口 module

HAL interface 保存非 owning context 和 ops table。clock、reset、IRQ、pinmux 与寄存器 ownership
留在 platform/board backend，不进入公共 handle。

## SPI 示例

```cpp
namespace platform::example {
    struct SpiContext {
        // Register base, vendor handle, DMA state, etc.
    };

    hal::Result init(void*, const hal::SpiConfig&) noexcept;
    hal::Result enable(void*) noexcept;
    hal::Result disable(void*) noexcept;
    hal::Result transfer(void*,
                         std::span<const util::u8>,
                         std::span<util::u8>) noexcept;

    inline constexpr hal::SpiOps spi_ops{
        .init = &init,
        .enable = &enable,
        .disable = &disable,
        .transfer = &transfer,
    };

    inline hal::SpiIoHandle bind(SpiContext& context) noexcept {
        return {&context, &spi_ops};
    }
}
```

该片段只展示绑定形状；函数签名以 `hal_spi.cppm` 为准。GPIO、UART、I2C 和 timer backend 使用
相同模式，但必须读取各自 interface module 的当前 ops 字段。

## 规则

- context 的生命周期覆盖所有 handle 调用；handle 不拥有 context 或 ops。
- backend 返回 `hal::Result`，不支持的操作返回 `Status::unsupported`。
- platform 负责 clock/reset/IRQ/pinmux；interface 不读取 board global。
- ops table 使用固定函数指针，不在调用路径动态分配。
- 需要 registry/init.graph 发布时，在装配层创建 binding，不把注册逻辑放入 ops callback。
