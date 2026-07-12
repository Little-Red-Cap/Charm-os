# charm-capability-map

## 用途

维护 `docs/capability_map.md` 的任务路由和证据入口。该文件是 supporting inventory，不是 Core capability registry。

## 使用前

1. 先读 `CONSTITUTION.md` 与 `docs/architecture/charm_core_contract.md`。
2. 如果是在新增 Capability Contract，先完成准入裁决；不要先向 map 添加名称。
3. 如果只是实现能力，确认其应留在 backend/driver/project，而不是 Core。

## 维护规则

- 每个条目必须指向真实源码、专题契约或可运行证据。
- 只维护按任务进入的短路由，不恢复“完整能力表”。
- 不使用无统一证据来源的 `stable/draft/planned/active` 手工状态。
- 不固定 Core/System/IO 等 taxonomy；目录分组不等于 Core 分类。
- generated capability map 只是正则扫描 inventory；capability 计数为 `0` 时必须视为不完整结果。
- 行为、错误、生命周期和平台限制留在专题契约，不复制到 map。

## 输出要求

- 说明新增或调整了哪个任务路由。
- 给出对应源码/契约/证据。
- 若涉及新 Core 名词，附 Constitution 裁决；没有裁决则明确保持 implementation 状态。
