# codegen

> status: `supporting`
>
> 用于模块、接口和实现骨架生成；不替代 rules、架构评审或现有 contract。

## 生成前

- 明确真实 consumer、所有者、目标行为和不改范围；
- 读取现有源码、CMake、测试和同目录惯例；
- 对关键歧义先列方案和取舍，不直接生成代码；
- 判断能力归属、初始化、错误、时间源、资源和执行上下文边界。

## 生成与实现

1. 先建模 domain、类型、接口、ownership 和失败语义。
2. 先生成最小正确骨架，再补参数检查和错误路径。
3. 优先使用强类型、值语义句柄、固定容量容器、`std::span`、`enum class` 和现有 Result；
   不用注释或调用顺序隐藏关键语义。
4. 不绕过 `init.graph`、`io.registry`、错误模型或时间源；跨上下文使用显式 ingress。
5. 第三方 SDK、寄存器和 legacy C 接口只在边界适配，不污染核心模块。

## 验证与输出

记录实际命令、结果、未验证项和残余风险；行为变化同步对应文档。需要结构化输出时使用
[`codegen template`](../../templates/codegen-output.md)，执行阶段见
[`codegen workflow`](../../workflows/codegen-workflow.md)。
