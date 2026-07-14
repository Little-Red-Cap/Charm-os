# Charm C++ 工程契约

> `status`: `supporting`
>
> `scope`: Charm C++ 语言基线、target 能力边界、Modules、ABI、资源成本与迁移纪律
>
> `authority`: 架构与 Core 语义服从 [`CONSTITUTION.md`](../../../CONSTITUTION.md) 和专题 contract；
> 构建事实服从 CMake、toolchain 与真实 target

本文属于 `Implementation / Tool` 层的工程契约，不定义 Charm Core，也不要求为了使用新语法而重写
稳定代码。项目级分层见 [`charm-architecture.md`](charm-architecture.md)，具体模块行为仍由对应 contract
和源码定义。

## 当前基线

- 根构建使用 C++26 并启用 C++ Modules 扫描；leaf component 也声明 `cxx_std_26`。
- 主机参考工具链是 GCC 16 + w64devkit，默认路径和覆盖变量由
  [`host-gcc16-w64devkit.cmake`](../../../cmake/toolchains/host-gcc16-w64devkit.cmake) 定义。该文件当前只
  检查 compiler 是否存在，不强制精确版本。
- 部分 H747、resident App 与独立 smoke 仍显式使用 C++23；C++26 是仓库默认和迁移方向，不是“所有
  target 已完成迁移”的事实声明。
- 根构建尚未统一设置 `CMAKE_CXX_STANDARD_REQUIRED` 与 `CMAKE_CXX_EXTENSIONS`；实际 language mode
  必须从 target 配置和编译命令确认，不能只根据根变量推断。
- bare-metal target 通常使用 freestanding 配置，并关闭 exceptions、RTTI、thread-safe statics 与
  `cxa_atexit`；准确选项见 toolchain 和
  [`CharmTargetConfig.cmake`](../../../cmake/CharmTargetConfig.cmake)。
- Host、QEMU、工具、第三方适配和 MCU target 的标准库、线程、分配器与启动环境不同。语言版本相同
  不代表 library/runtime 能力相同。

任何新特性必须同时通过 compiler、standard library、link/runtime 和目标执行环境验证。只在 Host
编译通过，不能证明可进入跨 target 接口。

## 工程域

### 可移植接口与公共实现

- 接口表达行为、ownership、lifetime、单位、容量和失败语义，不泄漏 Host-only handle、allocator、
  thread 或标准库实现身份。
- 公开 template 使用窄 concept 约束真实操作；concept 只约束 C++ 形状，不冒充 Charm Capability
  Contract 或运行期 provider 发现。
- non-owning view 优先使用 `span`、`string_view` 或等价窄视图，但调用方必须保证底层对象生命周期。
- C++ module interface 是源码/构建边界，不是稳定 binary ABI。跨 image、编译器或语言边界使用明确的
  C ABI、wire layout 或专题 ABI。

### Hosted 与离线工具

- 可以使用动态容器、线程、filesystem、异常和完整标准库，但必须有明确 ownership、输入上限与失败
  处理，不得迫使 portable 或 MCU 接口采用相同机制。
- packer、inspector、codegen 和测试可以为可诊断性付出额外成本；其输出格式必须由 schema、wire
  contract 或 consumer 定义，而不是由 Host C++ object layout 定义。
- Hosted 便利实现不能成为真实板能力证据，也不能掩盖 target 缺失的 library/runtime 支持。

### Freestanding、实时与硬件路径

- ISR、DMA callback、scheduler/trap ingress、音频 callback 等 bounded path 不执行无界分配、阻塞
  IO、异常展开或不可控析构工作。
- 固定容量不是全仓默认；容量、overflow 行为与存储位置由 target 和执行上下文决定。
- 动态初始化应优先改为 `constexpr`、`constinit` 或显式 init。关闭 thread-safe statics 的 target 不得
  依赖并发安全的 function-local static 初始化。
- `volatile` 只表达特定硬件访问要求，不提供线程同步、DMA ownership 或 cache coherency。
- DMA/MMIO 接口必须说明 alignment、memory region、读写方向、cache clean/invalidate、完成/取消边界
  和 buffer ownership 归还。

## 语言设施准入

采用语言或标准库设施前，必须回答：

1. 它消除了哪些错误状态、重复逻辑或运行期开销；
2. 哪些 target、compiler 与 standard library 实际支持它；
3. 它是否改变 ABI、对象布局、异常/分配行为或初始化顺序；
4. 它对 Flash、RAM、stack、运行时间、编译时间和 module cache 有何影响；
5. 失败时能否局部回退，而不复制第二套公共模型。

具体使用规则：

- `constexpr`/`consteval` 用于稳定输入、查表、验证和生成，不把设备状态或外部输入伪装成编译期事实。
- concept/`requires` 用于缩小 template 接口和改善诊断，不为普通非模板 API 增加类型体操。
- `optional`、`expected`、variant 或项目 Result 按专题失败语义选择，不强制全仓统一成某个容器。
- ranges/view、coroutine 和 owning callable 必须额外审查 lifetime、隐藏状态、分配、取消和代码尺寸；
  未证明 bounded behavior 前不得进入实时路径。
- contracts、reflection 等仍依赖实验性 compiler/library 支持的设施，先放在 hosted probe 或局部实验；
  未通过受影响 target 的正反例前，不成为 portable interface 前置条件。
- 不因语法更新而替换行为稳定、成本明确且已有证据的实现。

## Modules 与构建

- 新 Charm C++ 代码优先接入现有 module/target，不为一次重构新增平行聚合入口。
- target 应通过 `target_compile_features` 或自身构建契约声明所需标准，不依赖偶然继承的全局变量；
  compiler/toolchain 专属选项留在对应 toolchain 或窄 target profile。
- module interface 只 export consumer 需要的最小声明；implementation detail 留在实现单元、partition
  或私有 header。
- C、CMSIS、HAL、vendor 与第三方 header 应在最小 adapter 范围引入。涉及 language linkage 时，
  优先放入 global module fragment 或独立 implementation TU，不让污染扩散到公共 interface。
- BMI/GCM 只属于当前 compiler、flags、宏、include path 和 source revision。不得跨 build directory、
  preset 或 toolchain 复制复用；CRC/linkage 异常先核对配置和依赖，再 clean-first 重建。
- `export import`、聚合 module 和 source collector 变化必须检查实际依赖方向，不能用 re-export 隐藏
  ownership 违规。
- 已知 linkage 事件及排查顺序见
  [`cpp_modules_stdlib_linkage_conflicts.md`](../../architecture/cpp_modules_stdlib_linkage_conflicts.md)；
  局部 workaround 不自动升级为全仓规则。

## ABI 与边界

- 不把 C++ name mangling、vtable、exception、RTTI、STL container 或 host `uintptr_t` object layout 当作
  跨 compiler、跨 image、跨核或持久化格式。
- C ABI、App ABI、syscall、packet、Store、ELF 和 ModuleX 各自服从专题 contract；C++ wrapper 不能
  静默改变 wire shape、calling convention、alignment 或 ownership。
- exception 不跨 C/HAL callback、App entry、syscall 或 IPC 边界；失败在边界处转换为明确状态。
- `void*`、裸指针和函数指针可用于 C ABI、HAL callback、type erasure 与 non-owning view，但应局部
  封装并标明 lifetime、nullability 与解绑规则。
- `.h/.c/.cpp` 对 C、startup、vendor SDK 和第三方库仍是合法实现形式；Modules 不是删除这些边界的理由。

## 成本与证据

“零成本抽象”是待验证主张，不是设计理由。影响 bounded path、公共 template 或固件 image 的改动，
按风险提供以下一种或多种证据：

- map/section 与最终 binary size；
- stack、静态存储和动态分配上限；
- benchmark、周期或最坏延迟；
- 关键生成代码/反汇编；
- module 编译时间、实例化数量和诊断质量；
- Host、QEMU、real board 中与主张匹配的运行证据。

证据只覆盖对应 target、配置和 workload。Host benchmark、build-only 或单个 demo 不能替代真实板时序、
DMA、cache 或中断证据。

## 迁移纪律

- 不进行“全仓一次性 C++26 改写”。每批修改只解决一个可说明的问题，并保留可复测行为。
- 目录/ownership 迁移、公共 API 变化和语言现代化尽量分开提交，避免无法判断回归来源。
- 优先顺序是：统一 target/build 事实，修复 ownership/lifetime 和未定义行为，收窄 C/HAL 边界，减少
  重复与无类型宏，再采用能证明收益的新设施。
- 局部 target 暂不支持某设施时，兼容层留在 implementation/build 边界；不得复制第二套公共 API。
- 大规模迁移开始前先选 Host、QEMU、一个 freestanding target 建立 feature probe 与回归基线，再按
  module/owner 小批推进。

## 审查清单

- 该代码在哪些 target 与执行上下文运行；当前 toolchain 是否真实验证；
- ownership、lifetime、容量、alignment、初始化和失败行为是否显式；
- 使用的是语言能力，还是未确认存在的 standard library/runtime 能力；
- 编译期建模是否减少真实状态，而不是转移复杂度；
- ABI/wire/layout 是否仍由专题 contract 控制；
- 抽象成本是否有与风险相称的 size、map、benchmark 或机器证据；
- 是否存在更小、同样可验证且更符合现有 owner 的实现。

早期技术取舍保留在
[`embedded_cpp_retained_notes.md`](../../archive/project-guidance-and-tracking-v0/embedded_cpp_retained_notes.md)，
仅供追溯，不覆盖本契约。
