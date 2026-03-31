# 程序映像最小设计（Program Image v0）

目标：从“注册入口函数 + spawn”的世界，迈向“可加载程序映像 + 统一入口 ABI”的世界，为未来 ELF 接入预留轨道。

## 1. 目标与非目标

目标：
- 定义 `ProgramImage` 抽象，拆分 `load/start/spawn` 责任边界。
- 规定最小入口 ABI（`argc/argv/envp`、栈布局、退出路径）。
- 定义 argv/envp 内存布局规则与容量边界。
- 给出与未来 ELF 对接的兼容策略。

非目标（v0 不做）：
- 完整 ELF 解析与动态链接器。
- 完整 POSIX 进程语义（`fork`/`execve`）。
- 完整 libc/syscall 覆盖清单。

## 2. 现状与问题

现状路径是“注册入口函数 + spawn”：
- 入口是函数指针，而不是程序映像。
- 入口 ABI 没有被独立定义（只是函数调用）。
- `spawn` 同时承担“进程模型”和“程序加载”职责。

问题：
- 无法表达真实的程序映像（文件/ELF）。
- argv/envp 的布局/生命周期不明确。
- 退出路径和 wait 状态只是函数返回包装。

## 3. 抽象：ProgramImage

引入一个中间层，承载“可执行映像”信息：

```cpp
// 仅示意，不是最终代码
enum class ImageKind : u8 { registered, flat, elf };

struct ProgramImage {
    ImageKind kind{};
    const char* name{nullptr};
    const void* entry{nullptr};   // v0: 入口函数指针
    u32 text_size{0};
    u32 data_size{0};
    u32 bss_size{0};
    u32 stack_size{0};
    u32 flags{0};
};
```

要点：
- v0 允许 `registered` 映像：用函数指针包装为 ProgramImage。
- v1 对接 ELF 时，仅替换加载器，不改变入口 ABI。

## 4. 分层：load / start / spawn

分层规则：

```cpp
// v0 API 轮廓
Result<ProgramImage> load_image(ImageRef ref);
Result<ProcessId> start_image(const ProgramImage& image, const SpawnConfig& cfg);
Result<SpawnResult> spawn(const SpawnConfig& cfg); // POSIX-ish wrapper
```

职责边界：
- `load_image`：解析或装载映像，返回 ProgramImage。
- `start_image`：完成 argv/envp/栈/stdio 绑定与入口调用。
- `spawn`：POSIX 风格入口，调用 load/start 并做 errno 映射。

## 5. 最小入口 ABI

入口签名：

```cpp
using ImageEntry = int (*)(int argc, char** argv, char** envp);
```

规则：
- `argc/argv/envp` 由 loader 构建并传入。
- 入口返回值映射为 `WaitStatus.code`。
- v0 暂不引入 signal/异常终止分支。

## 6. argv/envp 内存布局规则

最小布局：

```
| argv ptrs | envp ptrs | strings blob |
```

规则：
- 所有字符串都拷贝到连续 blob。
- `argv[i]` 指向 blob 内部。
- `argv[argc] == nullptr`；`envp[n] == nullptr`。

容量策略（v0）：
- 固定上限（例如 4 KB），超限返回 `ENOSPC`/`buffer_overflow`。
- 由 loader/ProcessImage 构建时负责检查。

## 7. 用户栈与退出路径

栈：
- v0 可由执行环境提供固定栈。
- `stack_size` 仅用于约束或统计，不强制分配策略。

退出：
- 入口返回值 -> `WaitStatus.code`。
- `WaitStatus.kind = exited`。

## 8. 与 ELF 的兼容策略

必须保持的兼容点：
- 入口 ABI 不变（`argc/argv/envp`）。
- `ProgramImage` 仍是统一执行描述。

可替换点：
- `load_image` 内部实现从 `registered` 过渡到 `elf`。
- `ProgramImage.entry` 从函数指针过渡到加载后的入口地址。

## 9. 迁移路径（v0 -> v1）

v0：
- 增加 `program_image` 模块，提供 ProgramImage 与最小 load/start 接口。
- `registered` adapter：把现有注册入口包装成 ProgramImage。

v1：
- ELF loader 接入 `load_image`。
- 增加 `ImageKind::elf`，保持 start/spawn 不变。

## 10. 验收目标（v0）

必须走 ProgramImage 路径的最小样本：
- `hello`
- `argv_dump`
- `exit_code`

要求：
- argv/envp 经过统一布局构建。
- `spawn -> load_image -> start_image` 链路可追踪。

