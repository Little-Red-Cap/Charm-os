# POSIX ProgramImage 契约

状态：supporting contract。

`ProgramImage` 是 POSIX 执行面内部的已解析程序描述。它统一入口 ABI 与启动路径，
但不统一文件格式、存储位置或 loader 实现。

## Entry 契约

`ProgramImage` 保存 image kind、name、显式 entry ABI、与 ABI 对应的唯一 entry，以及可选
stack/flags 提示。Kind 只用于分类；枚举中存在某种 kind，不表示 resolver 已提供自动发现或
加载路径。

当前入口 ABI：

```cpp
int main(int argc, char** argv);               // main_argv_v0
int main(int argc, char** argv, char** envp);  // main_argv_envp_v1
```

`validate_program_image()` 拒绝空入口、`none` ABI，以及同时设置两种 entry 的对象。
`invoke_program_main()` 只按显式 ABI 分发，不猜测函数签名。

## Catalog 与解析

`ProgramCatalog<MaxExecs>` 是固定容量注册表：

- name 与 entry 必须有效
- 重名返回 `exist`
- 容量耗尽返回 `buffer_overflow`
- 查找允许完整名称或路径末段匹配

ELF memory registry 保存不拥有的 name/bytes 视图，调用方负责 image bytes 生命周期。

Resolver 当前按以下顺序解析：

1. `elfmem:<name>`：从 `ElfMemRegistry` 取 bytes 并调用 ELF loader。
2. `elf:<path>`：按 cwd 解析文件并调用 ELF loader。
3. registered name：按 exact/PATH 规则查询 catalog。
4. 普通文件候选：在允许 ELF 执行且存在文件服务时尝试加载 ELF。

ModuleX 不是 resolver 的独立文件协议；调用方先 materialize `ProgramImage(kind=modulex)`，再注册到
catalog 供 name 解析。

## ELF loader

ELF loader 接收 image bytes 与调用方提供的 load buffer。当前实现：

- 识别 ELF32/ELF64 与 little-endian header
- 校验 program-header 范围和大小
- 只 materialize `PT_LOAD`
- 拒绝 `filesz > memsz`、越界、重叠、对齐错误与 W+X segment
- 复制 file bytes，并将 `memsz - filesz` 清零
- 要求入口落在已加载 segment 内
- 产出 `main_argv_envp_v1` 的 `ProgramImage`

loader 不拥有输入或输出 buffer，不分配执行区，也不实现动态链接。平台/`ProcService` 必须提供
容量、对齐和可执行性正确的 load region。

## ModuleX loader

ModuleX loader 校验 image，解析 dependency/external，执行 relocation，并从指定 symbol 或 image
entry 取得 `main_argv_envp_v1` 入口。`entry_override` 只用于测试，不是 image 入口协议。

基础 ModuleX image/layout 语义见
[`ModuleX_格式草案.md`](../../Modules/system/modulex/ModuleX_格式草案.md)。该实现的原生 `Symbol`
布局含 `uintptr_t`，跨架构 artifact 必须使用明确 wire layout。

## 生命周期与失败边界

- `ProgramImage.name`、ELF input、ModuleX header 和 registry bytes 都可能是不拥有视图。
- loader 输出必须在 `start_image()` 完成前保持有效。
- 解析失败使用 `util::Errc`；当前 ELF 细分校验最终多数收敛为 `invalid_arg`。
- 找不到名称或路径返回 `noent`；能力未启用返回 `not_supported`。
- loader 成功不等于程序已运行；argv/envp、fd、退出与 wait 由 `ProcService` 处理。

## 与 Resident AppImage 的边界

POSIX `ProgramImage` 的 entry 是 `main(argc, argv[, envp])`。Resident App 原型的 `AppImage`
进入 `CharmAppMainFn` 并消费 `CharmAppApi`。二者共享 ELF/ModuleX 格式时仍必须使用各自 loader
和 entry ABI，不能直接交换 loaded image。

验证入口位于 `Examples/posix/tests/posix.programs.exec.tests.cppm`。早期设计稿保留在
[`../archive/posix-v0/`](../archive/posix-v0/README.md)。
