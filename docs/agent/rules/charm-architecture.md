# Charm Architecture Rules

用途：
- 定义 Charm 项目的具体工程约束与架构纪律。
- 约束 AI 在 Charm 项目中进行实现、审查、评估时，必须遵守统一的模块依赖、IO、错误模型、初始化与时间源规则。
- 作为项目级具体规则来源，优先于泛泛的“常见工程实践”。

适用场景：
- 代码审查
- 架构评审
- 模块设计与装配
- 判断某段实现是否符合 Charm 项目要求
- 判断某个改动是否破坏 IO/Runtime/Foundation/Domain 纪律

不适用场景：
- 纯协作方式讨论
- 单纯的现代 C++ 技术哲学讨论而不涉及 Charm 项目
- 任务步骤定义本身

相关文件：
- `collaboration.md`
- `embedded-modern-cpp.md`

---

# 1. 总体定位

本文件描述 Charm 项目中的项目级约束。

它回答的问题不是“业界一般怎么做”，而是：

- Charm 项目要求怎么做
- 哪些边界不能破坏
- 哪些装配方式必须统一
- 哪些工程行为必须保持一致

当外部常见实践与本项目要求冲突时，应优先本文件。

---

# 2. 语言与运行时要求

默认要求包括：

- 禁止异常
- 禁止 RTTI
- 默认使用 C++ Modules
- 以 C++23 / C++26 语义为主

平台相关限制：

- 除 Windows 平台外，禁止 `std::thread` / `std::mutex` / `std::atomic`
- Windows 平台可按需开放，但不得污染 MCU 路径

---

# 3. 容器与内存纪律

在 MCU 实时路径中：

- 禁止 `std::vector`
- 禁止 `std::string`
- 禁止 `std::deque`
- 禁止动态分配

在 MCU 非实时路径中：

- 动态容器仅可在明确上限与边界时受限使用

长生命周期对象优先：

- 固定容量容器
- 池化
- 显式容量管理

Kernel、驱动、实时回调中，不应引入动态分配。

---

# 4. C++ 特性使用约束

默认鼓励：

- `enum class`
- `constexpr`
- `consteval`
- `std::array`
- `std::span`
- `std::string_view`
- `concept`
- `requires`
- `std::optional`

默认受限：

- `std::vector` / `std::string` / `std::deque`
- `std::chrono`（仅限 PC/工具；核心统一使用 `charm.system.clock`）
- `std::function`（优先函数指针或 `function_ref`）

默认禁止（除 Windows 平台）：

- `std::thread`
- `std::mutex`
- `std::atomic`
- 异常
- RTTI

---

# 5. 输出与日志要求

默认要求：

- 使用 `out::format` / `out::printf`
- 不使用 `printf` / `snprintf`
- 日志使用 `out::logger` / `trace_core`
- `trace_core` 只写入，不承担格式化

在实时路径中：

- 只允许写入原始字节
- 不做格式化
- 不做分配

对外入口应统一走：

- `out.api`

---

# 6. 模块依赖纪律

分层关系为：

- Foundation
- Runtime
- IO
- Domain（如 UI / Media）

依赖方向要求：

- Foundation 不依赖 Runtime / IO / Domain
- Runtime 只依赖 Foundation
- IO 只依赖 Foundation / Runtime
- Domain 可依赖 Foundation / Runtime / IO
- 禁止反向渗透

AI 在评审时，应优先检查是否存在分层反向污染。

---

# 7. 文件系统与 IO 规则

默认要求：

- `close/flush` 必须走统一回收路径
- 只读打开不得隐式创建文件
- 错误码必须向上透传，不得吞掉
- 根挂载应显式 `clear_mounts()` 后再 `add_mount("/", mount)`

这类规则属于项目具体实现纪律，不应被一般性经验替代。

---

# 8. 统一错误模型

错误码统一使用：

- `util::Errc`

结果类型统一使用：

- `util::Result<T>`

若需要额外上下文，应使用：

- `Errc + stage/context`

不应为局部方便而随意自建风格各异的 Error 结构体。

---

# 9. IO 通道与协议纪律

默认要求：

- `io::Channel` 的 `read/write` 必须非阻塞
- 不得返回 `Ok(0)`
- 资源暂不可用时，必须返回 `Errc::would_block`

协议层禁止：

- busy-spin
- 阻塞等待
- 睡眠等待
- 自定义超时循环

等待与超时必须统一走：

- Kernel / EDA
- `io.reactor`

通道获取应优先通过：

- `io.registry.open("...")`
- cap id
- RuntimeContext 注入

默认通道已移除，不应再依赖隐式默认通道。

输入采样统一通过：

- `input.service`

业务层不应直接轮询硬件。

---

# 10. 初始化与能力装配

初始化统一走：

- `init.graph`

禁止：

- 在入口手写初始化顺序

装配要求：

- 核心底座放入 `CoreSystemChain`
- 板级能力放入 `BoardChain`
- `extra nodes` 仅用于服务/应用类节点
- 底座能力不得混入 `extra nodes`

内核能力统一通过：

- `charm.system.caps::SystemCaps`

不得散落临时 Caps 结构体。

---

# 11. 模块导出约定

IO / Runtime / Foundation 层的 API：

- 只导出统一入口
- 不暴露内部细节
- 不让内部实现细节渗透到外部依赖面

---

# 12. CMake 与第三方依赖

默认优先顺序：

1. `find_package`
2. 本地路径
3. `FetchContent`

要求：

- 避免在核心模块引入必须联网的依赖
- 第三方库必须可开关控制
- 第三方库应可编译排除

特定约束：

- FatFs 固定使用 `Modules/thirdparty/fatfs`
- 覆盖目录使用 `CHARM_FATFS_ROOT`

---

# 13. 构建与提交要求

默认要求：

- 尽量构建确认后再提交
- 架构核心代码应分别进行 PC 与 MCU 构建验证

---

# 14. 统一时间源

全局时间源统一为：

- `charm.system.clock`

业务、协议、驱动不得自建：

- `now_ms`
- `now_us`

时间获取应通过：

- Clock 注入
- `ClockCaps::TimeSource`

`BoardCaps` 必须提供 `ClockDesc`。

bringup 只负责注入，不应在模块内部偷取平台时间。

---

# 15. 评审时的默认检查点

AI 在代码审查或架构评审时，应优先检查：

- 是否违反模块分层
- 是否绕过 `init.graph`
- 是否违反 IO 非阻塞纪律
- 是否吞掉错误码
- 是否错误使用默认通道
- 是否引入不统一的错误模型
- 是否在业务/协议/驱动中自建时间源
- 是否错误引入不受控第三方依赖

---

# 16. 与其他规则的关系

本文件定义的是 Charm 项目的具体工程纪律。

- 若任务是协作方式问题，优先看 `collaboration.md`
- 若任务是技术哲学与现代嵌入式 C++ 方向问题，优先看 `embedded-modern-cpp.md`
- 若任务是代码评审、代码生成、架构审查，则在本文件基础上结合具体 skill 执行