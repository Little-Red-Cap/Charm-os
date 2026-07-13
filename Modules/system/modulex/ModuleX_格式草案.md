# ModuleX v2 实现格式

## 文档状态

- `status`: `supporting`
- `scope`: `module_core/view/link/loader/registry` 当前内存 image 语义
- `source`: 同目录 C++ module

本文描述当前实现，不承诺跨架构稳定 wire ABI。Resident App pack/inspect 工具使用显式 32-bit wire
struct；`module_core::Symbol::value` 使用 `uintptr_t`，因此 host 与 32-bit target 的原生 struct 大小
可能不同，不能直接互换序列化结果。

## Header 与布局

`ImageHeader` 固定 `magic=0x43484D4D`、`version=2`，包含：

- `flags` 与 `entry_offset`；
- text/ro/data/rel/sym/str/dep 的 offset 和 size；
- `bss_size` 与可选 `image_size`。

offset 为 `0` 时，`module_view` 按 header、text、ro、data、reloc、symbol、string、dependency 的
顺序推导布局。非零 offset 允许显式布局。`image_size` 非零时限制 validator 的有效 image 范围。

`ImageFlags` 当前定义 `xip_text/xip_ro/xip_data`。基础 view 只识别 flag；是否具备可执行映射、cache
维护和段 ownership 由具体 loader/runtime 决定。`bss_size` 也不会由基础 `Loader` 自动分配或清零。

## Symbol、dependency 与 relocation

`Symbol` 包含 string-table `name_offset`、`value`、`size`、`kind` 和 `flags`。kind 为 local、global
或 external；local/global 的 value 相对 text base，external value 由 resolver 写入绝对地址。

`Dependency` 保存 name/version 的 string-table offset。`module_link` 只调用 dependency resolver；
版本匹配策略由调用方或 `module_registry::match_version()` 提供。registry 当前支持 exact、比较运算、
`^` 主版本、`~` 主+次版本和 `x/*` wildcard。

`Reloc` 字段为 target image offset、type、symbol index 与 addend：

| type | 写入值 |
|---|---|
| `none` | 不修改 |
| `abs_addr` | `symbol_address + addend` |
| `rel32` | `symbol_address + addend - (target + 4)` |

`sym_index=0xFFFFFFFF` 使用 image base。`rel32` 超出 signed 32-bit 范围时失败。target 可落在
text/ro/data；调用方必须确保对应 materialized region 可写。

## 校验与加载

`module_view::validate()` 检查：

- image/header、magic、version、text 与 entry；
- section 是否落在有效 image 范围；
- reloc/symbol/dependency size 是否为 struct 整数倍；
- relocation target 与 symbol index 范围。

它不验证 section 互不重叠、字符串必有 NUL、segment 权限、BSS/XIP 可执行性或具体 entry ABI。
`module_loader::Loader` 只返回 entry、symbol reader 和 dependency view；external binding、dependency
resolution、relocation及最终 runtime ABI 由上层显式执行。

## 边界

- POSIX loader 将结果解释为 `main(argc, argv, envp)`。
- Resident App loader 将入口解释为 `charm_app_main(api, argc, argv)`。
- 相同 ModuleX bytes 不代表两种 runtime entry ABI 可以互换。
- 需要跨架构 artifact 时使用显式 wire struct，不用 `sizeof(modulex::Symbol)` 推导文件布局。
