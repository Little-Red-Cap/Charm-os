# Tool Surface Reduction v0 归档

> status: `archived`

早期脚本/schema 数量盘点、行数阈值、opening-flow pilot、shared-definition 候选与“第一批”排期已删除。
它们只反映当时工作区快照，不能作为当前 CI gate、风险排序或兼容要求。

现行规则：

- [`script_surface_reduction_governance_v0.md`](../../system/script_surface_reduction_governance_v0.md)
- [`schema_surface_reduction_governance_v0.md`](../../system/schema_surface_reduction_governance_v0.md)

保留的最低判断是：

- 脚本负责编排、采集与验证，不拥有系统 verdict；
- schema 约束公开 artifact shape，不以字段名发明语义；
- projection、compare 与 runtime evidence 保持各自 ownership；
- 新 artifact 不默认复制 exporter/validator/smoke/inspect/report/compare 全家族；
- 文件数量和行数只是审计信号，不是删除或拆分门禁。

历史 inventory、pilot 文件名和统计数据从 Git 历史追溯。
