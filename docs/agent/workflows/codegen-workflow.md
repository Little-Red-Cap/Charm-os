# Codegen Workflow

> status: `supporting`
>
> 入口与规则加载顺序见 [`codegen route`](../routes/codegen.md)。本页只规定代码生成的执行顺序。

## 执行

1. 核对目标、约束、所有权和验收方式；只有会改变行为边界的歧义才暂停确认。
2. 读取现有源码、CMake、测试和同目录惯例，优先复用已有接口。
3. 使用 [`codegen skill`](../skills/codegen/SKILL.md) 确认类型、错误、资源和装配边界。
4. 实现最小完整改动，同步必要文档，不顺带重构无关代码。
5. 运行与风险匹配的构建和测试，记录未验证项。
6. 输出结果、验证和风险；需要结构化说明时参考
   [`codegen template`](../templates/codegen-output.md)。

能力归属或 Core 准入问题应先切换到 [`architecture route`](../routes/architecture.md)。
