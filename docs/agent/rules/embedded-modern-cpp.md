# OnlyCore C++ 工程规则

> `status`: `supporting`
>
> `scope`: Core 公共 header、Host 证据和 CMake consumer

架构与 Core 语义服从 [`CONSTITUTION.md`](../../../CONSTITUTION.md) 和
[`charm_core_contract.md`](../../architecture/charm_core_contract.md)。本文只约束当前 OnlyCore 工程实现。

## 公共表面

- 公共入口只有 `Charm::core` 与 `core/capability/relations.hpp`。
- 公共类型只投影已裁决关系，不加入 resolver、Profile、Provider、Backend、registry 或平台事实。
- key 使用不同的 scoped enum 类型；字符串、数组下标和 provider identity 不参与关系判定。
- 公共 header 不依赖 OS、vendor、HAL、动态分配、异常或隐藏全局状态。

## 构建与消费者

- CMake target 通过 `target_compile_features` 声明 C++26，不依赖目录级编译选项。
- 示例必须链接 `Charm::core` 并通过公共 include 路径包含 header，不得直接添加仓库根目录绕过 target。
- Header-only 根构建只证明配置与导出表面成立；示例编译和运行才证明真实 consumer 可用。
- 编译器、标准库和运行环境是不同证据维度；Host 通过不外推到 QEMU 或真实板。

## 代码与验证

- 值类型保持显式、可比较、无所有权；新增字段必须说明语义、失败行为和 consumer。
- 正例之外必须覆盖 duplicate、missing、unknown、mismatch 和 invalid 等适用失败路径。
- 修改公共关系后运行治理检查、关系 Host 测试、MVP Host 测试和 portable source boundary。
- `git diff --check` 与成功配置不能替代编译或运行证据。

早期跨模块、ABI、实时、DMA 和 C++ Modules 规则只从快照分支或 Git 历史追溯，不属于当前
OnlyCore 活动工程表面。
