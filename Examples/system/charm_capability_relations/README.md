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
- `projection`：ResolvedBinding 投影为显式 app context 和只读 evidence，不依赖 init/runtime。

运行：

```powershell
.\Examples\system\charm_capability_relations\run_host_ci.ps1
```

当前 gate 是 Host Clang；GCC 可作为兼容性补充运行。QEMU、真实板和旧 board log 不属于当前
OnlyCore 验证范围。
