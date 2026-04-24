# ProgramImage / ModuleX 适配草案（v0）

目的：利用现有 ModuleX 的装载/重定位/符号/依赖能力，作为 ProgramImage 的 v0 适配路径，为后续 ELF 接入打基础。

## 适配思路

ModuleX 已具备：
- 镜像头格式与入口定位（`modulex::Loader::load`）
- 符号绑定与依赖校验（`modulex::Linker::bind_externals` / `validate_deps`）
- 重定位（`modulex::Linker::relocate`）

因此可以将 ModuleX 视为 ProgramImage 的一种 `ImageKind::modulex`，先实现“内存镜像 -> ProgramImage”的转换，再由 `start_image` 执行入口。

## 最小接口（建议）

- 输入：`const modulex::ImageHeader*`（已映射到内存）
- 输出：`ProgramImage`
- 约束：入口 ABI 先按 `ImageEntry(int,char**,char**)` 处理

对应实现已落到：
- `Modules/io/posix/posix.program_image_modulex.cppm`

## 处理流程（v0）

1. `Loader::load(header)` 校验 magic/version 与入口偏移
2. （可选）`validate_deps` / `validate_deps_ctx` 做依赖检查
3. （可选）`bind_externals` 绑定外部符号
4. `relocate` 应用重定位
5. 组装 `ProgramImage`，entry 指向 ModuleX entry

## 语义边界

- 这是“程序映像适配”，不是“用户态 ABI 完整实现”
- argv/envp/stdio 仍由 `start_image` 负责
- 依赖与外部符号解析策略由调用方提供

## 后续演进

- v1：支持 ModuleX 作为 `load_image` 的候选分支
- v2：对齐 ELF loader 的入口 ABI 与重定位语义
- v3：替换或兼容 ModuleX 与 ELF（以 ProgramImage 为统一入口）
