# 项目规范（持续维护）

目标：把分散的子库统一到同一套工程约束中，减少“各用各的”。

> 说明：本规范分为两类。
> - A 通用 C++ 要求：语言/运行时/编码习惯
> - B Charm 架构要求：模块装配/通道/错误模型/依赖纪律

## A) 通用 C++ 要求

### A.1 语言与运行时

- 禁止异常（`throw`）、RTTI（`dynamic_cast`）
- 除 Windows 平台外，禁止 `std::thread` / `std::mutex` / `std::atomic`
- 默认使用 C++ Modules，C++23/26 语义为主

### A.2 容器与内存

| 类别 | MCU 实时路径 | MCU 非实时 | PC/工具 |
| --- | --- | --- | --- |
| `std::vector/string/deque` | ❌ 禁止 | ⚠️ 限制使用 | ✅ |
| 固定容量容器 | ✅ `core/service/*` | ✅ | ✅ |
| 动态分配 | ❌ 禁止 | ⚠️ 有上限 | ✅ |

补充：
- Kernel/驱动/实时回调禁止 `new/delete`。
- 长生命周期对象优先池化或固定容量容器。

### A.3 C++ 特性使用清单

#### 允许

- `enum class`、`constexpr`、`consteval`
- `std::array`、`std::span`、`std::string_view`
- `concept`、`requires`（优先替代虚表）
- `std::optional`（可替换为轻量方案时再替）

#### 受限

- `std::vector/string/deque`：仅限非实时路径，需明确上限
- `std::chrono`：仅限 PC/工具或通过 TimeSource 统一
- `std::function`：优先用函数指针或 `function_ref`

#### 禁止（除 Windows 平台）

- `std::thread` / `std::mutex` / `std::atomic`
- 异常（`throw`）、RTTI（`dynamic_cast`）

> Windows 平台可按需开放，但不得影响 MCU 路径。

### A.4 虚表与多态

- MCU 路径默认禁用虚表，优先 `concept + template`。
- 仅在 PC/工具端允许虚表，且必须标注平台限定。

### A.5 字符集与编码

- 对外接口统一 UTF-8。
- 文档允许非 ASCII，代码默认 ASCII（除非已有非 ASCII 且有必要）。

## B) Charm 架构要求

### B.1 输出与日志

使用 `out::format` / `out::printf` 替换 `std::printf` / `std::snprintf`

| 项目 | 规则 | 说明 |
| --- | --- | --- |
| 格式化 | ✅ `out::format` / `out::printf` | 禁止 `printf/snprintf` |
| 日志 | ✅ `out::logger` / `trace_core` | `trace_core` 只写入、不可格式化 |
| 实时路径 | ⚠️ 只允许写入原始字节 | 不做格式化、不分配 |

### B.2 模块依赖

分层：Foundation → Runtime → IO → Domain(UI/Media)

| 层 | 允许依赖 | 禁止依赖 |
| --- | --- | --- |
| Foundation | 无 | Runtime/IO/Domain |
| Runtime | Foundation | IO/Domain |
| IO | Foundation/Runtime | Domain |
| Domain | Foundation/Runtime/IO | 反向渗透 |

### B.3 文件系统与 IO

- `close/flush` 必须走统一回收路径。
- 只读打开禁止隐式创建文件。
- 错误码向上透传，不吞掉。

### B.4 统一错误模型

- 错误码统一使用 `util::Errc`。
- 结果类型统一使用 `util::Result<T>`（`expected<T, Errc>`）。
- 需要额外上下文时，使用 `Errc + stage/context` 并显式命名，不再自建 Error 结构体。

### B.5 IO 通道与协议纪律

- `io::Channel` 的 `read/write` 必须非阻塞，禁止返回 `Ok(0)`。
- 资源暂不可用必须返回 `Errc::would_block`。
- 协议层禁止 busy-spin/阻塞/睡眠；等待/超时必须走 Kernel/EDA + `io.reactor`。
- 通道获取优先 `io.registry.open("io.console0")` 或 cap id；默认通道仅兼容。

### B.6 初始化与能力装配

- 初始化统一走 `init.graph`，禁止入口手写顺序。
- 核心底座放入 `CoreSystemChain`，板级能力放入 `BoardChain`。
- `extra nodes` 仅允许服务/应用类节点，禁止底座能力进入 extra。

### B.7 模块导出约定

- IO/Runtime/Foundation 层的 API 只导出统一入口，不暴露内部细节。

### B.8 CMake 与第三方

- 优先 `find_package` → 本地路径 → `FetchContent`。
- 避免在核心模块引入“必须联网”的依赖。
- 第三方库必须有开关控制（可编译排除）。
- FatFs 固定使用 `Modules/thirdparty/fatfs`，覆盖目录用 `CHARM_FATFS_ROOT`。

### B.9 注意

- 本项目使用 C++ Modules 需使用 CMake 4.0 及以上版本。

### B.10 提交与构建要求

- 尽量进行构建后确认再提交。
- 如果是架构核心代码，需分别进行 PC 和 MCU 构建。
