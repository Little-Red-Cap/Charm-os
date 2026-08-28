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

三个编译期负例分别拒绝 Contract/Requirement、Contract/Provision 和 Requirement/Provision 复用同一
key 类型。负例 target 不进入默认构建；Host runner 必须观察到构建失败，并匹配该 domain 的稳定诊断标识。
默认构造禁令由 `model.cpp` 的三条 `std::is_default_constructible_v` 类型性质断言证明。

运行：

```powershell
.\Examples\system\charm_capability_relations\run_host_ci.ps1
```

本地 runner 与首次全绿远端证据 [run 33140346354](https://github.com/Little-Red-Cap/Charm-os/actions/runs/33140346354)
均覆盖 Host Clang 与 GCC。QEMU 的 MVP 运行证据从相邻
[`charm_capability_mvp`](../charm_capability_mvp/README.md) 进入。后续状态以每个 commit 对应的 Actions run
为准；真实板和旧 board log 不属于当前 OnlyCore 验证范围。
