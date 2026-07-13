# HAL ops backend 模板

## 文档状态

- `status`: `supporting`
- `scope`: 当前 `{ctx, ops}` HAL handle 的 backend 绑定
- `source`: `hal_spi.cppm`、`hal_gpio.cppm` 及同目录接口 module

HAL interface 保存非 owning context 和 ops table。clock、reset、IRQ、pinmux 与寄存器 ownership
留在 platform/board backend，不进入公共 handle。

## SPI 示例

```cpp
module;

#include <span>

export module platform.example.spi;

import hal_core;
import hal_spi;
import util.core;

namespace platform::example {
    struct SpiContext {
        // Register base, vendor handle, DMA state, etc.
    };

    hal::Result init(void* raw, const hal::SpiConfig& config) noexcept {
        auto& context = *static_cast<SpiContext*>(raw);
        (void)context;
        (void)config;
        return hal::ok();
    }

    hal::Result enable(void* raw) noexcept {
        (void)raw;
        return hal::ok();
    }

    hal::Result disable(void* raw) noexcept {
        (void)raw;
        return hal::ok();
    }

    hal::Result transfer(void* raw,
                         std::span<const util::u8> tx,
                         std::span<util::u8> rx) noexcept {
        (void)raw;
        (void)tx;
        (void)rx;
        return hal::ok();
    }

    inline constexpr hal::SpiOps spi_ops{
        .init = &init,
        .enable = &enable,
        .disable = &disable,
        .transfer = &transfer,
    };

    hal::SpiIoHandle bind(SpiContext& context) noexcept {
        return {&context, &spi_ops};
    }
}
```

GPIO、UART、I2C 和 timer backend 使用相同模式，但必须以各自 interface module 的当前 ops 字段
为准，不复制本示例的 SPI 操作集。

## 规则

- context 的生命周期覆盖所有 handle 调用；handle 不拥有 context 或 ops。
- backend 返回 `hal::Result`，不支持的操作返回 `Status::unsupported`。
- platform 负责 clock/reset/IRQ/pinmux；interface 不读取 board global。
- ops table 使用固定函数指针，不在调用路径动态分配。
- 需要 registry/init.graph 发布时，在装配层创建 binding，不把注册逻辑放入 ops callback。
