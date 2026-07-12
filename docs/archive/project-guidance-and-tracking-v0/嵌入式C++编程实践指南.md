# 嵌入式 C++ 编程实践指南

本文用于回答一个具体问题：在 Charm 的嵌入式代码里，C++ 应该怎么写。

它不是替代 [`项目C++编码要求.md`](../../project/standards/项目C++编码要求.md)，而是把其中的“允许 / 受限 / 禁止”落到更具体的接口、内存、模板、错误处理、测试代码与平台边界场景中。

## 0. 适用范围

本文按代码所在位置区分规则强度。不要把 Windows 测试代码的自由度带进可移植库，也不要把 MCU 实时路径的限制机械套到 host 工具。

| 场景 | 说明 | 规则强度 |
| --- | --- | --- |
| 库本体 | `Modules/*` 中会进入 Charm 主库、可被 MCU 链接的代码 | 最严格 |
| MCU 实时路径 | ISR、驱动回调、协议处理、调度路径、IO reactor 回调、帧/音频/输入等实时链路 | 最严格 |
| MCU 非实时路径 | 初始化、配置解析、一次性装配、离线准备步骤 | 严格，但允许有界妥协 |
| 平台 / HAL / 第三方适配层 | MMIO、DMA、vendor SDK、C ABI、启动代码边界 | 允许局部 escape hatch |
| Windows / host 测试 | 单元测试、host smoke、模拟器、验证工具 | 可使用 host C++ 能力 |
| 文档 / reference / generated | 参考材料、生成物、第三方对照 | 不作为当前编码规范入口 |

## 1. 规则词汇

- **必须**：默认要求。违反即视为设计或实现问题。
- **禁止**：不得在对应场景出现，除非本文明确给出 escape hatch。
- **推荐**：默认选择。若不用，需要能说明更合适的本地理由。
- **允许**：可使用，但要控制范围，不得扩大为默认风格。
- **仅限**：只能在列出的场景中出现。

## 2. 总体原则

1. 接口优先表达语义，不暴露实现步骤。
2. 能在类型系统表达的约束，不写到注释里。
3. 能在编译期决定的事项，不拖到运行期分支。
4. 资源成本必须可解释、可裁剪、可验证。
5. MCU 代码默认不依赖动态内存、异常、RTTI、线程运行时。
6. Host 测试可以更自由，但不得改变库本体接口与约束。

## 3. 场景能力矩阵

| 能力 / 特性 | 库本体 | MCU 实时路径 | MCU 非实时路径 | 平台 / HAL 适配 | Windows / host 测试 |
| --- | --- | --- | --- | --- | --- |
| `new/delete`、`malloc/free` | 禁止 | 禁止 | 受限；仅初始化 / 装配阶段，且容量上限和失败路径显式 | 仅限已封装的固定池 | 允许 |
| `std::vector/string/deque` | 禁止作为默认存储 | 禁止 | 受限；必须明确容量上限，不得进入实时链路 | 不推荐 | 允许 |
| 固定容量容器 | 推荐 | 必须优先 | 推荐 | 推荐 | 允许 |
| 异常 | 禁止 | 禁止 | 禁止 | 禁止穿过边界 | 允许 |
| RTTI / `dynamic_cast` | 禁止 | 禁止 | 禁止 | 禁止 | 允许 |
| 虚函数 / 虚表 | 默认禁止 | 禁止 | 受限 | 受限 | 允许 |
| `std::thread/mutex/atomic` | 禁止，Windows 专用除外 | 禁止 | 禁止 | 仅限平台封装内部 | 允许 |
| `std::span` / `std::string_view` | 推荐 | 推荐 | 推荐 | 推荐 | 推荐 |
| `constexpr` / `consteval` | 推荐 | 推荐 | 推荐 | 推荐 | 允许 |
| `concept` / `requires` | 推荐 | 推荐 | 推荐 | 推荐 | 允许 |
| 宏表达语义 | 禁止 | 禁止 | 禁止 | 仅限条件编译 / ABI | 允许但不推荐 |
| 裸指针 + 长度接口 | 禁止 | 禁止 | 禁止 | 仅限 C / MMIO / DMA 边界 | 允许适配外部 API |

## 4. 数据视图与 buffer 规则

### 4.1 buffer + length

除 C ABI、MMIO、DMA 描述符、第三方 SDK 适配层外，禁止把裸指针和长度拆开作为公共接口。

```cpp
// 禁止：调用者必须靠注释理解 len 的单位、生命周期和可写性。
util::Result<usize> write(uint8_t* buffer, usize len);

// 推荐：span 同时表达连续区间、长度和可写性。
util::Result<usize> write(std::span<std::byte> buffer);
util::Result<usize> read(std::span<const std::byte> buffer);
```

使用规则：

- 可写二进制缓冲区使用 `std::span<std::byte>`。
- 只读二进制缓冲区使用 `std::span<const std::byte>`。
- 协议字节需要参与数值比较、位运算时，可以使用 `std::span<const uint8_t>`。
- 固定长度数据优先使用 `std::array<T, N>`，对外传参时降为 `std::span<T, N>` 或 `std::span<T>`。
- 不允许用 `nullptr + 0` 表示空缓冲；使用空 `span`。
- 不允许把 `span` 存进长生命周期对象，除非对象语义明确是“借用视图”，且生命周期由类型名或构造方式表达。

### 4.2 文本与字符串

库本体默认不拥有动态字符串。

| 场景 | 推荐 |
| --- | --- |
| 只读文本参数 | `std::string_view` |
| 固定容量文本 | 项目固定容量字符串 / `std::array<char, N>` |
| C 字符串边界 | 在 adapter 层转换为 `string_view` |
| Host 测试与工具 | 可用 `std::string` |

禁止在库本体中把 `const char*` 当作普通文本接口。只有在 C ABI、启动符号、第三方 SDK 或编译器约束处允许。

### 4.3 输出缓冲区

库函数不得私自分配输出缓冲。默认由调用者提供 `span`。

```cpp
struct EncodeResult {
    usize written;
};

util::Result<EncodeResult> encode(Frame const& frame, std::span<std::byte> out);
```

如果返回的是输入缓冲的子区间，可以返回 `std::span<const std::byte>`；但必须保证其生命周期不超过输入。

### 4.4 volatile、MMIO 与 DMA

`volatile` 不是线程同步工具，只用于硬件可见副作用。

允许场景：

- MMIO 寄存器访问。
- DMA 可见内存区域的边界封装。
- 编译器必须保留的硬件访问顺序。

要求：

- `volatile` 不得泄漏到业务接口。
- MMIO 地址转换必须隔离在平台 / HAL 层。
- DMA buffer 必须用类型表达对齐、缓存一致性、读写方向和生命周期。

## 5. 参数与返回值

### 5.1 参数表达

| 语义 | 写法 |
| --- | --- |
| 必需只读对象 | `T const&` |
| 必需可变对象 | `T&` |
| 小型值语义对象 | `T` |
| 连续区间 | `std::span<T>` / `std::span<const T>` |
| 可选值 | `std::optional<T>` 或项目轻量 optional |
| 可选引用 | 优先用显式语义类型；不得随意使用裸指针 |
| 多参数配置 | 命名 `struct` |
| 模式 / 状态 / 类别 | `enum class` 或 tag type |

禁止用 `int mode`、`bool enable`、`void* context` 这类弱语义参数承载 domain 含义。

```cpp
// 禁止：参数顺序和含义只能靠记忆。
void configure(int channel, int mode, bool dma, uint32_t timeout_ms);

// 推荐：类型说明调用意图。
enum class ChannelMode { Rx, Tx, Duplex };
struct TimeoutMs { uint32_t value; };

struct ChannelConfig {
    ChannelId channel;
    ChannelMode mode;
    DmaPolicy dma;
    TimeoutMs timeout;
};

util::Result<void> configure(ChannelConfig config);
```

### 5.2 返回值

运行期失败必须通过返回值表达，不使用异常。

| 语义 | 写法 |
| --- | --- |
| 可能失败 | `util::Result<T>` |
| 只有成功 / 失败 | `util::Result<void>` |
| 不存在不是错误 | `std::optional<T>` 或项目轻量 optional |
| 多个返回量 | 命名 result struct |
| 程序员错误 | contract / assert 类机制 |

禁止返回魔法值表达错误，例如 `-1`、`nullptr`、`0xffff`，除非处于 C API adapter，并且立即转换为项目错误模型。

## 6. 内存与生命周期

### 6.1 动态内存

库本体和 MCU 实时路径禁止依赖通用堆。MCU 非实时路径只有在初始化、配置解析、一次性装配等阶段才允许受控动态行为，并且必须有明确容量上限和失败路径。

推荐替代：

- `std::array<T, N>`
- `std::span<T>`
- `core/service/*` 固定容量容器
- 显式对象池、句柄池、arena
- 调用者提供存储

受控 arena / pool 的要求：

1. 容量是编译期常量或由平台配置显式给出。
2. 分配失败路径有明确错误返回。
3. 不引入碎片化或不可解释的长期增长。
4. 生命周期边界明确，不跨模块泄漏所有权。

### 6.2 所有权

公共接口不得使用拥有语义不清的裸指针。

| 场景 | 推荐 |
| --- | --- |
| 借用对象 | `T&` / `T const&` |
| 借用数组 | `std::span<T>` |
| 资源所有权 | 值语义句柄、固定池 handle |
| 可释放资源 | RAII wrapper |
| C 边界资源 | adapter 内部封装 |

RAII 是推荐实践，但析构函数不得抛出异常，不得隐藏不可预测阻塞，不得在实时路径触发动态释放。

### 6.3 静态初始化

必须避免有副作用的动态初始化。

推荐：

- `constexpr`
- `consteval`
- `constinit`
- 显式 `init.graph` 装配

禁止在全局对象构造中注册服务、启动硬件、分配内存或依赖初始化顺序。

### 6.4 递归

MCU 实时路径、kernel、driver、协议解析默认禁止递归。若算法天然递归，必须改写为显式栈，并说明最大深度。

Host 工具和测试可以递归，但不得复用到 MCU 库本体。

## 7. 模板、concept 与构造约束

### 7.1 什么时候使用模板

推荐使用模板的场景：

- 编译期配置。
- 能力注入。
- 类型级状态机。
- 避免虚表的静态多态。
- 固定容量、固定维度、固定协议参数。

不推荐使用模板的场景：

- 只是为了少写一个函数重载。
- 数据本来是运行期输入，却强行变成类型参数。
- 会造成大量无意义实例化，且没有成本收益。

### 7.2 公共模板必须约束

公共模板必须用 `concept` / `requires` 表达所需能力。

```cpp
template<class T>
concept ByteSink = requires(T sink, std::span<const std::byte> data) {
    { sink.write(data) } -> std::same_as<util::Result<usize>>;
};

template<ByteSink Sink>
util::Result<void> flush_to(Sink& sink, std::span<const std::byte> payload) {
    auto written_result = sink.write(payload);
    if (!written_result) {
        return util::unexpected(written_result.error());
    }

    auto written = *written_result;
    return written == payload.size()
        ? util::Result<void>{}
        : util::unexpected(util::Errc::short_write);
}
```

禁止让模板错误信息暴露为几百行 substitution failure。约束是接口的一部分。

### 7.3 构造模板必须防止劫持

带转发引用的构造模板很容易劫持复制 / 移动构造，公共类型中必须谨慎使用。

```cpp
class Packet {
public:
    Packet(Packet const&) = default;
    Packet(Packet&&) = default;

    template<class R>
        requires ByteRange<R> && (!std::same_as<std::remove_cvref_t<R>, Packet>)
    explicit Packet(R&& bytes);
};
```

规则：

- 构造模板必须 `explicit`，除非隐式转换是类型语义的一部分。
- 必须排除自身类型，避免劫持 copy / move。
- 必须用 concept 限定输入类别，例如 range、byte span、config object。
- 不允许写“万能构造函数”再在函数体里用 `if constexpr` 猜测类型。

### 7.4 编译期配置

硬件能力、协议特性、buffer 容量、调度策略等稳定配置，优先进入类型或模板参数。

```cpp
template<usize RxSize, usize TxSize>
    requires (RxSize > 0 && TxSize > 0)
struct UartBufferConfig {
    static constexpr usize rx_size = RxSize;
    static constexpr usize tx_size = TxSize;
};
```

运行期数据不得伪装成模板参数。模板参数应该表达“配置和结构”，不是表达“每次调用的数据”。

### 7.5 `auto` 与约束

允许使用 `auto` 简化局部变量。公共接口中使用 abbreviated template 时必须带约束。

```cpp
// 不推荐：公共接口看不出能力要求。
void mount(auto& fs);

// 推荐。
void mount(FileSystemLike auto& fs);
```

## 8. 多态与类型擦除

### 8.1 默认选择

优先级：

1. `concept + template`
2. `std::variant` / 明确 sum type
3. 固定容量 type erasure
4. 函数指针 / `function_ref` 风格借用回调
5. 虚函数

### 8.2 虚函数

MCU 实时路径禁止虚函数。库本体默认不使用虚表。

允许场景：

- Host 工具 / Windows 测试。
- 不进入 MCU 的模拟接口。
- 已明确证明虚表成本可接受，且无法用静态多态表达的边界层。

要求：

- 必须标注平台限定或 escape hatch 原因。
- 不得在热路径以虚调用替代可静态解析的能力。
- 析构语义必须清楚；不得通过基类指针拥有 MCU 资源。

### 8.3 回调

实时路径不使用 `std::function`。

推荐：

- 函数指针。
- 模板回调对象。
- 不拥有的 `function_ref` 风格视图。
- 固定容量 small-function，且容量和失败路径显式。

Host 测试可以使用 `std::function`。

## 9. 错误处理、契约与断言

### 9.1 错误处理

库本体禁止异常。错误返回统一使用项目错误模型。

```cpp
util::Result<Frame> decode(std::span<const std::byte> bytes);
```

规则：

- 可恢复错误用 `util::Result<T>`。
- 缺失不是错误时用 optional。
- 错误码向上传递，不吞掉。
- adapter 层要把第三方错误码立即转换为 `util::Errc` 或命名错误语义。

### 9.2 异常

允许异常的场景仅限 host 测试、测试框架、Windows 工具与不进入 MCU 目标的辅助代码。

要求：

- 不得在库本体 API 中出现 `throw` 语义。
- 不得让异常穿过 C ABI、module 边界或 MCU 可链接代码。
- 测试可以用异常让断言和 fixture 更清晰，但不要倒逼库接口改成异常风格。

### 9.3 契约

区分两类问题：

- 外部输入、设备状态、资源不足：返回 `Result`。
- 调用者违反前置条件：使用 contract / assert。

禁止用 assert 处理正常运行期错误，例如 IO 暂不可用、解析失败、buffer 太小。

## 10. 并发、时间与等待

### 10.1 并发

除 Windows / host 测试外，禁止直接使用 `std::thread`、`std::mutex`、`std::atomic`。

MCU 并发能力必须通过项目 runtime、kernel、EDA、reactor 或平台封装表达。

### 10.2 等待与超时

协议层禁止 busy-spin、阻塞等待、睡眠和自定义超时循环。

要求：

- IO 暂不可用返回 `Errc::would_block`。
- 等待和超时走 Kernel / EDA / `io.reactor`。
- 时间源统一通过 `charm.system.clock` capability 注入。
- 除 PC / 工具端外，不直接使用 `std::chrono` 获取时间。

## 11. 数值、单位与状态

### 11.1 整数类型

外设寄存器、协议字段、文件格式、ABI 边界必须使用固定宽度整数。

业务计数、容量、索引必须明确单位。不要用裸 `int` 表达状态、大小、时间或模式。

```cpp
struct Bytes { usize value; };
struct Milliseconds { uint32_t value; };
struct Hertz { uint32_t value; };
```

### 11.2 枚举与状态

使用 `enum class` 表达离散状态。禁止使用宏常量或裸整数表达模式。

状态迁移复杂时，优先使用类型级状态机或显式状态表，不把状态藏在多个 bool 中。

## 12. 宏、条件编译与 ABI

宏只允许用于：

- include guard 兼容场景。
- 条件编译。
- 编译器 / 平台属性封装。
- C ABI 或启动文件约束。
- 极少数不能用 `constexpr` 表达的预处理需求。

禁止用宏生成业务接口、隐藏控制流、模拟泛型、承载 domain 语义。

条件编译必须尽量集中在平台层。库本体不应散落大量 `#ifdef _WIN32` 或芯片型号判断。

## 13. C++ Modules 与文件组织

默认使用 C++ Modules。

要求：

- 模块接口优先表达 domain 边界，而不是机械复刻 `.h/.cpp`。
- 第三方 C 头、平台头、寄存器头应隔离在 global module fragment 或 adapter 模块。
- 聚合导出走统一 `charm.*.cppm` 入口。
- 文件拆分必须来自语义边界、编译隔离或测试替换需求。

## 14. 平台 / HAL / 第三方边界

以下写法只允许出现在边界层，并且范围要最小：

- `reinterpret_cast`
- `const_cast`
- `void*`
- 裸指针 + 长度
- `extern "C"`
- 编译器属性
- `volatile`
- packed / aligned 布局控制
- 第三方错误码

边界层职责：

1. 接收不安全接口。
2. 立即转换为强类型、`span`、`Result`、`enum class`。
3. 不把脏接口传播到核心逻辑。
4. 必要时登记到 [`escape_hatches.md`](../../project/escape_hatches.md)。

## 15. Windows / host 测试规则

Host 测试和工具的目标是更快验证库行为，因此允许使用：

- 动态内存。
- 异常。
- RTTI。
- `std::string` / `std::vector` / `std::filesystem`。
- `std::thread` / `std::mutex` / `std::atomic`。
- 测试框架提供的 fixture、matcher、mock。

但必须遵守隔离要求：

1. Host-only 代码必须放在测试、工具、示例或明确的 host adapter 中。
2. Host-only 能力不得出现在库本体公共接口。
3. 测试 helper 不得被 MCU 目标链接。
4. 不得为了测试方便放宽库本体错误模型、内存模型或并发模型。
5. 若测试需要动态构造输入，应在调用库本体前转换成 `span`、`string_view` 或固定语义对象。

```cpp
// Windows 测试中允许。
std::vector<std::byte> bytes = load_fixture(path);

// 调用库本体时仍然使用库接口风格。
auto frame = decode(std::span<const std::byte>{bytes.data(), bytes.size()});
```

## 16. 推荐实践速查

| 问题 | 默认写法 |
| --- | --- |
| buffer 参数 | `std::span<std::byte>` / `std::span<const std::byte>` |
| 文本参数 | `std::string_view` |
| 固定容量集合 | `std::array` 或 `core/service/*` 固定容量容器 |
| 错误返回 | `util::Result<T>` |
| 可选返回 | optional |
| 模式选择 | `enum class` / tag type |
| 多参数配置 | 命名 `struct` |
| 编译期能力 | `concept` / `requires` |
| 编译期配置 | 模板参数 + requires |
| 多态 | `concept + template` |
| 回调 | 函数指针 / function_ref 风格 / 模板回调 |
| 时间源 | `charm.system.clock` |
| IO 暂不可用 | `Errc::would_block` |
| 第三方 C API | adapter 层隔离 |
| host 测试数据 | `std::vector` 可用，调用库时转 `span` |

## 17. 禁止清单

库本体和 MCU 路径禁止：

- 通用动态分配。
- 异常和 RTTI。
- 公共接口使用裸指针 + 长度。
- 公共接口使用 `void*`。
- 用 `int` / `bool` 表达模式、单位、状态。
- 用宏承载语义。
- 协议层 busy-spin、阻塞、睡眠、自定义超时循环。
- 业务层直接轮询硬件。
- 业务层直接使用平台时间。
- 热路径使用 `std::function`。
- 实时路径使用虚调用。
- 全局对象做有副作用初始化。
- 用 assert 处理可恢复运行期错误。

## 18. 受限清单

以下内容允许但必须说明范围和理由：

- 虚函数：仅限 host、adapter 或有证据的非热路径。
- `reinterpret_cast`：仅限硬件 / ABI / 第三方边界。
- `volatile`：仅限硬件可见副作用。
- 受控 arena / pool：容量、失败路径、生命周期必须显式。
- `std::chrono`：仅限 PC / 工具；目标库使用系统 clock capability。
- `std::function`：仅限 host 或非实时工具代码。
- 递归：仅限 host 或已证明最大深度的非实时代码。

## 19. Escape hatch 要求

当必须违反默认实践时，必须满足：

1. 范围最小，优先限制在单个函数或单个 adapter 模块。
2. 注释写明 `ESCAPE_HATCH`、原因、替代方案为何不可用。
3. 有证据：硬件手册、ABI 要求、benchmark、第三方限制。
4. 登记到 [`escape_hatches.md`](../../project/escape_hatches.md)。
5. 不得污染核心模块公共接口。

注释模板：

```cpp
// ESCAPE_HATCH: Vendor SDK requires pointer + length C ABI.
// Alternative considered: span-based wrapper. The wrapper is provided by this adapter;
// raw pointer does not leave this file.
// Evidence: vendor_hal_uart_write(void*, size_t) signature.
```

## 20. Review 检查清单

提交或 review 时至少检查：

- 接口是否用类型表达了 buffer、单位、模式、所有权和错误。
- 是否存在裸指针 + 长度、`void*`、魔法整数、多个 bool 参数。
- 是否有动态分配、异常、RTTI、虚调用进入 MCU 路径。
- 模板公共接口是否有 concept / requires。
- 构造模板是否会劫持 copy / move。
- 错误是否用 `Result` 传递，是否误用 assert。
- 等待 / 超时是否走 runtime / reactor / clock capability。
- 第三方或平台妥协是否被 adapter 隔离。
- Host-only 代码是否和库本体隔离。
- 若改变契约、模块入口或行为，是否同步更新相关文档。
