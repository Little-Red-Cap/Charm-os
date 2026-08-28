# Charm Capability Relations

## 文档状态

- `status`: `supporting`
- `scope`: Capability Core relation v1 的 Host 语义、失败矩阵与 projection boundary
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

本目录验证公共 [`relations.hpp`](../../../Modules/core/capability/relations.hpp)。它不定义具体
Capability Contract、resolver、Provider、Profile、Backend 或全局 registry。

三组 CTest 分别验证：

- `model`：项目局部强类型 key、共享 Provision 与非权威 label；
- `resolution`：重复、缺失、未知和 contract mismatch；
- `projection`：已验证 Binding 投影为显式 app context 和只读 evidence，不引入同形结果类型。

四个编译期负例分别拒绝 Contract/Requirement、Contract/Provision 和 Requirement/Provision 复用同一
key 类型，以及三个关系类型的默认构造。负例 target 不进入默认构建，Host runner 必须观察到它们构建失败。

运行：

```powershell
.\Examples\system\charm_capability_relations\run_host_ci.ps1
```

本地 runner 已分别验证 Host Clang 与 GCC；workflow 已定义对应远端 jobs，首次 Actions 结果须以远端 run
为准。QEMU 的 MVP 运行证据从相邻 [`charm_capability_mvp`](../charm_capability_mvp/README.md) 进入。真实板和
旧 board log 不属于当前 OnlyCore 验证范围。
