# 项目规范（持续维护）

目标：把分散的子库统一到同一套工程约束中，减少“各用各的”。

## 1) 输出与日志

使用`out::format` / `out::printf`替换 `std::printf` / `std::snprintf`

| 项目 | 规则 | 说明 |
| --- | --- | --- |
| 格式化 | ✅ `out::format` / `out::printf` | 禁止 `printf/snprintf` |
| 日志 | ✅ `out::logger` / `trace_core` | `trace_core` 只写入、不可格式化 |
| 实时路径 | ⚠️ 只允许写入原始字节 | 不做格式化、不分配 |

## 2) 容器与内存

| 类别 | MCU 实时路径 | MCU 非实时 | PC/工具 |
| --- | --- | --- | --- |
| `std::vector/string/deque` | ❌ 禁止 | ⚠️ 限制使用 | ✅ |
| 固定容量容器 | ✅ `core/service/*` | ✅ | ✅ |
| 动态分配 | ❌ 禁止 | ⚠️ 有上限 | ✅ |

补充：
- Kernel/驱动/实时回调禁止 `new/delete`。
- 长生命周期对象优先池化或固定容量容器。

## 3) 模块依赖

分层：Foundation → Runtime → IO → Domain(UI/Media)

| 层 | 允许依赖 | 禁止依赖 |
| --- | --- | --- |
| Foundation | 无 | Runtime/IO/Domain |
| Runtime | Foundation | IO/Domain |
| IO | Foundation/Runtime | Domain |
| Domain | Foundation/Runtime/IO | 反向渗透 |

## 4) 字符集与编码

- 对外接口统一 UTF-8。
- FAT LFN/UTF-16 处理放在 `fs` 层。
- 文档允许非 ASCII，代码默认 ASCII（除非已有非 ASCII 且有必要）。

## 5) 文件系统与 IO

- `close/flush` 必须走统一回收路径。
- 只读打开禁止隐式创建文件。
- 错误码向上透传，不吞掉。

## 5.1) 统一错误模型

- 错误码统一使用 `util::Errc`。
- 结果类型统一使用 `util::Result<T>`（`expected<T, Errc>`）。
- 需要额外上下文时，使用 `Errc + stage/context` 并显式命名，不再自建 Error 结构体。

## 6) C++ 特性使用清单

### 允许

- `enum class`、`constexpr`、`consteval`
- `std::array`、`std::span`、`std::string_view`
- `concept`、`requires`（优先替代虚表）
- `std::optional`（可替换为轻量方案时再替）

### 受限

- `std::vector/string/deque`：仅限非实时路径，需明确上限
- `std::chrono`：仅限 PC/工具或通过 TimeSource 统一
- `std::function`：优先用函数指针或 `function_ref`

### 禁止（除 Windows 平台）

- `std::thread` / `std::mutex` / `std::atomic`
- 异常（`throw`）、RTTI（`dynamic_cast`）

> Windows 平台可按需开放，但不得影响 MCU 路径。

## 7) 虚表与多态

- MCU 路径默认禁用虚表，优先 `concept + template`。
- 仅在 PC/工具端允许虚表，且必须标注平台限定。

## 8) CMake 与第三方

- 优先 `find_package` → 本地路径 → `FetchContent`。
- 避免在核心模块引入“必须联网”的依赖。
- 第三方库必须有开关控制（可编译排除）。

## 9) 模块导出约定

- IO/Runtime/Foundation 层的 API 只导出统一入口，不暴露内部细节。

## 10) 注意
- 本项目使用C++ Module需使用 CMake 4.0及以上版本
