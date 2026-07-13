# Architect Review Workflow

> status: `supporting`
>
> 入口与权威顺序见 [`architecture route`](../routes/architecture.md)。本页只规定架构评审的执行顺序。

## 执行

1. 从源码、CMake、真实 consumer 和当前证据确认问题，不从目录或旧设计稿反推事实。
2. 使用 [`architect-review skill`](../skills/architect-review/SKILL.md) 回答 Constitution 六问。
3. 给出唯一裁决等级，明确所有者、边界、失败语义和至少一个反例。
4. 比较更小的局部方案；没有足够证据时保留为 implementation 或 exploration。
5. 指明结论应更新的源码、契约或归档入口。
6. 输出可参考 [`architecture template`](../templates/architect-review-output.md)，不重复背景叙述。

局部函数或 PR 缺陷应切换到 [`review route`](../routes/review.md)。
