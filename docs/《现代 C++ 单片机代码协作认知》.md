# 现代 C++ 单片机代码协作认知

> 本文不是风格指南，而是一份**用于约束在同一项目中的共同认知边界**。

---

## 0. 项目定位（最高优先级）

### 运行环境

- 无操作系统（Bare-metal）或最小化运行时
- 无依赖于动态内存分配的运行时
- 不依赖异常 / RTTI

### 约束条件

- 资源受限（Flash / RAM / 带宽）
- 性能敏感（实时性、功耗、响应延迟）

---

## 1. 世界观声明（必须接受）

本项目 **不考虑**：

- C 兼容性
- ABI 稳定性
- 旧编译器支持
- "更多人能用"

**目标语言**：C++23、C++26

**默认选项**：C++ Modules

### 唯一目标

> **语义正确 × 可验证 × 零成本抽象 × 可长期演进**

### 选择原则

如果某种写法：

- 更现代
- 更强类型
- 更可组合
- 更符合编译期建模

即使它：

- 激进
- 不直观
- 与传统嵌入式经验相悖

👉 **也尽量选择它**

---

## 2. 工程动机与问题意识

### 核心问题

**嵌入式开发是否只能停留在低抽象、弱类型、强经验依赖的阶段？**

### 本项目试图回答

> 嵌入式开发，是否也能具备现代软件工程中的**优雅性、可组合性与可验证性**？

### 我们的立场

- 承认并感谢物联网生态带来的基础设施红利：
  - 现有 MCU 性能的提升
  - 更完善的编译器与工具链
  - 大量成熟开源框架的经验积累

> 本项目不是从零开始造轮子，而是**站在巨人的肩膀上重新设计轮子的形态**。

---

## 3. 硬性约束与不推荐项

### 3.1 硬性禁止（绝不妥协）

以下内容在本项目中**绝对禁止**，出现即视为设计失败：

- ❌ 动态内存分配（`new` / `delete` / `malloc` / `free`）
- ❌ 异常（Exception）
- ❌ RTTI（`typeid` / `dynamic_cast`）
- ❌ 宏承担语义职责（宏仅用于条件编译和简单常量）
- ❌ 运行期分支模拟编译期选择
- ❌ 协议层 busy-spin/阻塞等待/自定义超时循环
- ❌ 协议层直接依赖 platform/hal/driver（必须走 `io.channel/io.reactor/io.registry`）
- ❌ 入口手写初始化顺序（必须走 `init.graph`）

### 3.2 强烈不推荐（尽量避免，需充分理由）

以下内容应当尽量避免，若必须使用需要明确的工程理由：

- ⚠️ `void*`（类型擦除应使用 variant / type erasure 技术）
- ⚠️ 裸语义指针作为接口参数（使用 `std::span` / 引用 / 值语义句柄）
- ⚠️ `.h / .cpp` 拆分作为默认结构（优先单模块文件）
- ⚠️ 通过注释说明参数含义（应使用强类型）
- ⚠️ 通过顺序或约定隐式传递语义（应使用命名参数或 tag type）

> **原则**：如果使用了"不推荐项"，必须能清晰说明为何无法使用推荐替代方案。

---

## 4. 结构与组织原则

### 默认结构

- 模块文件（C++ Module）为默认组织形式
- 接口即实现（inline / constexpr / consteval）
- 模块化思维而非物理文件拆分

### 文件拆分的充分理由

仅在以下情况下才拆分文件：

1. 编译单元必须隔离（如硬件寄存器访问）
2. 明确的 domain 边界（如不同硬件外设）
3. 需要独立测试或替换的抽象层

> 物理拆分不能代替语义分层。

---

## 5. 类型系统优先级（极其重要）

### 接口设计优先级（从高到低）

1. **强类型**（struct / class / enum class / tag type）
2. **值语义句柄**（handle-like value type）
3. **标准容器视图**（`std::array` / `std::span` / `std::ranges`）
4. **可选与错误处理**（`std::optional` / `std::expected`）

### 核心原则

> 如果一个接口需要文档才能安全使用，它的类型设计大概率是错误的。

### 反模式示例

```cpp
// ❌ 错误：语义不明确
void configure(uint8_t* buffer, size_t len, int mode);

// ✅ 正确：类型携带语义
enum class ConfigMode { Master, Slave };
struct ConfigBuffer { std::span<uint8_t> data; };
void configure(ConfigBuffer buffer, ConfigMode mode);
```

---

## 6. 设计方法论

### 6.1 Domain 优先

- **先建模"是什么"**，再考虑"怎么做"
- 行为是 domain 的投影，而不是函数堆砌
- 数据结构先于算法
- 接口表达语义而非操作步骤

### 6.2 编译期优先

**原则**：能在编译期解决的，不得拖到运行期

#### 鼓励使用

- `constexpr` / `consteval` 函数
- 类型级状态机（Type-state pattern）
- 能力注入（Capability Injection）
- 编译期配置（通过模板参数而非宏）

#### 反模式示例

```cpp
// ❌ 错误：运行期分支
void set_pin_mode(int pin, int mode) {
    if (mode == 0) { /* ... */ }
    else if (mode == 1) { /* ... */ }
}

// ✅ 正确：编译期类型选择
template<PinMode Mode>
constexpr void set_pin_mode(Pin<Mode> pin) {
    if constexpr (Mode == PinMode::Output) { /* ... */ }
    else if constexpr (Mode == PinMode::Input) { /* ... */ }
}
```

---

## 7. 资源与成本可见性

### 零成本抽象原则

每一份抽象必须满足：

- **零运行期开销**（Zero-cost abstraction）
- 所有成本在类型层面可推导
- 所有成本在编译期可裁剪

> 如果你无法解释"这段抽象在 MCU 上的真实成本"，那它不该存在。

### 成本检查清单

在设计抽象时，必须回答：

1. 这个抽象会增加多少 Flash 占用？
2. 这个抽象会增加多少 RAM 占用？
3. 这个抽象的运行期成本是多少时钟周期？
4. 编译器能否完全优化掉这个抽象？

---

## 8. 激进优先原则

### 决策规则

当面临「传统写法」vs「现代但激进写法」时：

- **必须选择后者**
- 宁可不兼容，也不妥协语义
- 不因"别人看不懂"而降低设计质量

### 但要注意

激进不等于炫技：

- ✅ 激进：使用类型系统表达更强的约束
- ❌ 炫技：为了用新特性而强行使用

---

## 9. 逃生舱口（有限妥协）

### 9.1 何时允许妥协

本项目承认现实世界的复杂性。以下情况下允许**有限的**妥协：

#### 9.1.1 硬件强制约束

- 硬件寄存器访问要求特定的内存布局
- DMA 要求特定的内存对齐
- 中断服务程序（ISR）的调用约定限制

**示例**：
```cpp
// ✅ 允许：硬件寄存器访问
struct [[gnu::packed]] HardwareRegister {
    volatile uint32_t control;
    volatile uint32_t status;
};

// 必须使用裸指针，因为硬件地址固定
inline HardwareRegister* const TIMER0 = 
    reinterpret_cast<HardwareRegister*>(0x40000000);
```

#### 9.1.2 第三方库/SDK 强制接口

- 芯片厂商提供的 HAL/SDK 只有 C 接口
- 第三方库（如 FreeRTOS）使用传统 C 风格
- 调试工具链要求特定的符号格式

**原则**：在边界处隔离，不污染核心逻辑

#### 9.1.3 性能关键路径的特殊优化

- 经过 benchmark 证明，类型安全版本确实有不可接受的开销
- 编译器无法优化掉抽象层

**要求**：
1. 必须有 benchmark 数据支撑
2. 必须写清楚性能对比
3. 必须有注释说明为何无法使用类型安全版本

---

### 9.2 如何隔离"脏"接口

#### 9.2.1 适配器模式（Adapter Pattern）

将不符合规范的接口封装在最小的适配层中：

```cpp
// ❌ 第三方 C 库的接口
extern "C" {
    int legacy_init(void* config, size_t len);
    int legacy_send(void* data, size_t len, int flags);
}

// ✅ 类型安全的适配层
namespace adapter {
    enum class SendFlags { Blocking, NonBlocking };
    
    struct Config {
        // 强类型配置
    };
    
    class LegacyDevice {
        // 隔离层：仅此处使用 void*
        std::expected<void, Error> init(const Config& cfg) {
            auto raw = serialize(cfg);
            if (legacy_init(raw.data(), raw.size()) != 0) {
                return std::unexpected(Error::InitFailed);
            }
            return {};
        }
        
        std::expected<void, Error> send(
            std::span<const uint8_t> data, 
            SendFlags flags
        ) {
            int raw_flags = (flags == SendFlags::Blocking) ? 0 : 1;
            if (legacy_send(
                const_cast<void*>(static_cast<const void*>(data.data())),
                data.size(),
                raw_flags
            ) != 0) {
                return std::unexpected(Error::SendFailed);
            }
            return {};
        }
    };
}

// ✅ 项目代码只使用适配层
void user_code() {
    adapter::LegacyDevice device;
    device.init(adapter::Config{/* ... */});
    device.send(my_buffer, adapter::SendFlags::Blocking);
}
```

#### 9.2.2 文件隔离原则

将妥协代码隔离在特定文件中，并明确标注：

```
src/
├── core/              # 核心逻辑，严格遵守规范
│   ├── protocol.cppm
│   └── state_machine.cppm
├── hardware/          # 硬件抽象层，允许部分妥协
│   ├── registers.cppm
│   └── dma.cppm
└── adapters/          # 第三方库适配层，隔离脏接口
    ├── freertos_adapter.cppm  // ⚠️ Contains legacy C interface
    └── vendor_sdk_adapter.cppm // ⚠️ Contains void* usage
```

#### 9.2.3 编译期标记

对妥协代码使用明确的标记：

```cpp
// ⚠️ ESCAPE_HATCH: Performance-critical path
// Benchmark shows type-safe version adds 15% overhead
// See: docs/benchmarks/spi_transfer.md
inline void fast_memcpy_u32(
    volatile uint32_t* dest,
    const uint32_t* src,
    size_t count
) {
    // 使用裸指针和 volatile 的特殊优化
}
```

---

### 9.3 妥协代码的审查清单

在引入妥协代码之前，必须回答以下问题：

#### 必答问题

1. **为什么无法使用规范方案？**
  - [ ] 硬件强制约束
  - [ ] 第三方库限制
  - [ ] 性能关键路径（需要 benchmark）

2. **妥协的范围有多大？**
  - [ ] 仅单个函数
  - [ ] 单个模块
  - [ ] 跨模块（❌ 需要重新评估设计）

3. **是否已经最小化妥协范围？**
  - [ ] 是否可以用适配器模式隔离？
  - [ ] 是否可以用模板/constexpr 减少运行期妥协？

4. **是否有明确的文档说明？**
  - [ ] 代码中有 `ESCAPE_HATCH` 注释
  - [ ] 有链接到详细设计文档/benchmark
  - [ ] 有计划何时移除（如果可能）

#### 妥协代码的生命周期

```cpp
// ⚠️ ESCAPE_HATCH: Legacy SDK compatibility
// TODO(2026-Q2): Remove when vendor releases v2 SDK with C++ interface
// Tracking: https://github.com/vendor/sdk/issues/1234
void legacy_wrapper() {
    // ...
}
```

---

### 9.4 绝对不允许的妥协

即使在"逃生舱口"中，以下情况也**绝对禁止**：

- ❌ 在核心业务逻辑中直接使用 `void*`
- ❌ 让妥协代码"污染"其他模块
- ❌ 因为"懒得改"而使用妥协方案
- ❌ 没有文档说明的妥协
- ❌ 妥协范围超过单个模块

### 9.5 妥协的审批流程

对于"不推荐项"的使用：

1. **自审**：完成 9.3 审查清单
2. **代码注释**：添加 `ESCAPE_HATCH` 标记和理由
3. **文档记录**：在 `docs/escape_hatches.md` 中记录
4. **定期复审**：每季度评估是否仍然必要

---

## 10. 工程美学宣言

> 嵌入式开发不是低级开发。  
> 资源受限不意味着思想受限。  
> MCU 不是妥协的理由，而是设计能力的放大器。

我们要回答的问题只有一个：

> **嵌入式开发，是否也可以优雅、克制、并且强大？**

本项目给出的答案是：

> **可以，而且必须如此。**
