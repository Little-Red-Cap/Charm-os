# Review Workflow

> status: `supporting`
>
> 入口与规则加载顺序见 [`review route`](../routes/review.md)。本页只规定 review 的执行顺序。

## 执行

1. 读取 diff、相关源码、CMake 接线和测试；不要从提交说明推断行为。
2. 使用 [`code-review skill`](../skills/code-review/SKILL.md) 与
   [`checklist`](../skills/code-review/checklist.md) 检查回归、边界和缺失测试。
3. 按严重性排列 findings，并给出文件和行号。
4. 没有 finding 时，明确说明残余风险和未运行的验证。
5. 输出格式可参考 [`review template`](../templates/review-output.md)，但不得为了套模板增加空段落。

能力归属、Core 准入或系统装配边界应切换到
[`architecture route`](../routes/architecture.md)；纯实现建议继续留在 code review。
