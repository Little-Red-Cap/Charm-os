# Codegen Examples

本文件提供 Charm 项目中代码生成任务的输出范式示例。  
这些示例用于帮助 AI 学会：

- 在写代码前先说明设计
- 先给正确骨架，再补实现
- 不在需求不明确时盲目生成代码
- 生成的代码如何体现 Charm 的规则

相关文件：
- `SKILL.md`
- `../../rules/collaboration.md`
- `../../rules/embedded-modern-cpp.md`
- `../../rules/charm-architecture.md`

---

# 示例 1：先讨论设计，再给代码

## 输入场景
用户请求：

> 帮我写一个 SPI 设备初始化接口。

但没有说明：
- 是否运行在 MCU 实时路径
- 能力从哪里注入
- 是否已有 `BoardCaps`
- 是否走 `init.graph`

## 参考输出

### 先对齐设计
这类接口在 Charm 里不适合直接先写“普通初始化函数”，因为有几个关键点还没定：

1. SPI 能力是走 `BoardCaps` 注入，还是通过独立 capability 暴露？
2. 初始化是进入 `init.graph`，还是只作为某个更高层节点的局部配置？
3. 这个接口是面向驱动层，还是面向上层设备抽象？

我先给两个方向：

#### 方案 A：能力注入优先
- SPI 底层能力放在板级能力层
- 上层只拿抽象后的 handle / descriptor
- 更符合 Charm 的装配方向

#### 方案 B：局部初始化函数
- 先提供一个局部 `init()` 接口
- 实现更快
- 但后续可能更难纳入统一装配链

我更推荐方案 A。  
如果你接受这个方向，我再基于它给你写代码骨架。

---

# 示例 2：生成正确骨架而不是“全都写满”

## 输入场景
用户请求：

> 给我一个输入采样服务的初版实现。

## 参考输出

### 设计说明
我先按 Charm 的现有方向，把输入采样建模为服务层，而不是让业务代码直接轮询硬件。  
这个设计会优先满足：

- 不在业务层直接碰硬件
- 时间统一来自 `charm.system.clock`
- 错误返回保持统一
- 输入来源通过注入获得，而不是内部偷取

### 代码草案

```cpp
export module input.service;

import std;
import charm.system.clock;
import util.result;

export namespace input {

    enum class SampleMode {
        Single,
        Continuous
    };

    struct SampleRequest {
        SampleMode mode;
        std::span<std::uint16_t> buffer;
    };

    struct SampleResult {
        std::size_t count;
        std::uint64_t timestamp;
    };

    class InputBackend {
    public:
        auto sample(std::span<std::uint16_t> buffer) -> util::Result<std::size_t>;
    };

    class InputService {
    public:
        InputService(InputBackend& backend, ClockCaps::TimeSource& clock)
            : backend_(backend), clock_(clock) {}

        auto collect(const SampleRequest& request) -> util::Result<SampleResult> {
            auto count = backend_.sample(request.buffer);
            if (!count) {
                return std::unexpected(count.error());
            }

            return SampleResult{
                .count = *count,
                .timestamp = clock_.now_us()
            };
        }

    private:
        InputBackend& backend_;
        ClockCaps::TimeSource& clock_;
    };

}