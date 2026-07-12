# Embedded Modern C++ Rules

> **文档状态：`supporting`**

本文件约束 C++ 设计判断，不定义 Charm Core 或具体模块行为。项目级架构边界见
[`charm-architecture.md`](charm-architecture.md)，具体 target 配置以 CMake/toolchain 为准。

## 当前语言事实

- 根构建当前使用 C++26 并启用 C++ Modules 扫描；
- H747 与多个 bare-metal toolchain 使用 `-fno-exceptions -fno-rtti -fno-threadsafe-statics`；
- Host、工具、第三方适配与 MCU target 的标准库、内存和线程能力不同；
- 某种类型或语言特性在仓库中出现，不等于它适合所有 target。

不要把某个 toolchain 的编译选项提升成跨平台 ABI，也不要让 Host 便利反向污染 MCU 实时路径。

## 设计原则

- 类型应表达所有权、单位、状态和失败边界，但不为“强类型”增加无消费者的包装层；
- 编译期处理稳定配置和结构约束，运行期处理真实输入、设备状态和动态选择；
- 值语义、`span`、`string_view`、`optional`、`expected`、concept 和 constexpr 按问题使用，
  不是必须凑齐的技术清单；
- 抽象应减少错误状态或重复逻辑，并能说明 Flash、RAM、运行时间和编译影响；
- 注释解释取舍和不变量，类型系统不能替代硬件时序、协议和生命周期文档。

## 资源与实时路径

- ISR、音频 callback、scheduler/trap ingress 等实时路径不得执行无界分配、阻塞 IO 或不可控工作；
- 固定容量不是全仓默认，容量策略由 target、执行上下文和失败行为决定；
- Host/离线算法可以使用动态容器，但必须有明确 ownership、失败处理和输入边界；
- atomic、锁和线程是否允许取决于执行域及 toolchain，不以平台名称一刀切；
- benchmark 或 map/size 证据用于判断成本，不能用“零成本”口号代替测量。

## 边界与互操作

- `void*`、裸指针和函数指针可用于 C ABI、HAL callback、type erasure 与 non-owning view，
  但应在边界处封装并明确 lifetime；
- `.h/.cpp` 对 C、vendor SDK 和第三方库仍是合法形式；Charm C++ module 代码优先使用现有 module 入口；
- 宏可用于 toolchain、feature gate 和 vendor 兼容，不承载可由类型/constexpr 表达的领域语义；
- runtime branch 在处理真实状态时是正确工具；只有稳定配置被重复解释时才考虑编译期选择；
- 不为追求新语法改写已稳定且可验证的实现。

## 错误与生命周期

- 不依赖异常的 target 必须使用显式返回、状态或 Result；具体模型由模块 contract 决定；
- 资源获取与释放路径必须成对，失败中途不能泄漏或留下半初始化状态；
- non-owning view 不得延长底层对象生命周期，异步 callback 必须有显式取消/解绑边界；
- 第三方或硬件强制接口的妥协应局部隔离，并记录范围和退出条件。

## 审查问题

- 该特性在哪些 target 和执行上下文运行；
- ownership、lifetime、容量和失败行为是否显式；
- 编译期建模是否减少真实状态，而不是转移复杂度；
- 抽象成本是否有 map、size、benchmark 或生成代码证据；
- C/HAL/第三方边界是否被限制在适配层；
- 是否存在更简单、同样可验证且更符合现有代码的方案。

旧技术宣言归档于
[`../../archive/agent-guidance-v0/embedded_modern_cpp_legacy.md`](../../archive/agent-guidance-v0/embedded_modern_cpp_legacy.md)。
