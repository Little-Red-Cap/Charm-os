# Charm Schemas

## 文档状态

- `status`: `supporting`
- `scope`: `schemas/` 机器可读协议与 sample 路由
- `authority`: 各 `*.schema.json`、producer 与 validator

本页不复制字段定义。这里的 Schema 文件是提纯前各专题的历史机器产物样例，不属于 OnlyCore 当前
实现入口，也不证明协议已闭环或运行事实成立。

## 内容与入口

- Artifact contract schema 定义机器产物的 identity 与 shape；对应 contract 解释语义和 ownership。
- Projection、summary 与 compare schema 只承载既有事实和 verdict，不重新判定上游结果。
- Shared definition 提供可复用的 envelope、reference 与 status shape；`examples/` 保存测试 fixture。

准确文件集合以本目录为历史记录；OnlyCore 当前没有 schema validator 或生成器入口。

## Samples

`schemas/examples/*.json` 是测试 fixture，不是运行证据、默认配置或产品 manifest。Sample 可以固定
正例 shape、静态引用和 consumer 输入，但不能证明真实 producer 已运行。

旧卫生检查脚本已随外围路线退役。

## 增改规则

- 修改公开 shape 时同步 producer、validator、sample 和真实 consumer；
- 新字段必须有来源、消费方、失败语义与兼容策略，不能只在 README 中解释；
- projection 不重新判定上游 verdict，compare 不伪造缺失 baseline/candidate；
- `v0/v1`、schema 数量和 sample 数量都不授予 Core、稳定 ABI 或实现成熟度；
- 优先复用已有 envelope/definition，避免为每个 route、workspace 或阶段旁白新增 schema。

Schema 准入与缩面规则见
[`evidence_surface_governance_v0.md`](../docs/system/evidence_surface_governance_v0.md)。
