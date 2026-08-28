# Core 关系示例

本目录只保留当前 OnlyCore 的关系语义证据。各示例的 `CMakeLists.txt`、源码和运行脚本是
局部权威；通过只证明对应关系在 Host 上可验证，不证明 Core 已独立构建。

## 入口

| 示例 | 用途 |
|---|---|
| [`charm_capability_relations`](charm_capability_relations/README.md) | Core 关系模型、解析和投影验证 |
| [`charm_capability_mvp`](charm_capability_mvp/README.md) | 最小能力组合验证 |
| [`charm_core_external_consumer`](charm_core_external_consumer/README.md) | 安装后的独立 CMake 消费验证 |
