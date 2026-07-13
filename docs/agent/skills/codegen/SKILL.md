# codegen

> status: `supporting`
>
> 用于模块、接口和实现骨架生成；不替代 rules、架构评审或现有 contract。

## 生成前

- 明确真实 consumer、所有者、目标行为和不改范围；
- 读取现有源码、CMake、测试和同目录惯例；
- 对会改变公共接口、ownership 或兼容性的歧义先列方案和取舍；
- 按专题 contract 确认初始化、错误、资源和执行上下文边界。

## 生成与实现

1. 先确定最小接口、ownership、生命周期和失败语义。
2. 复用目标目录的类型、错误和装配模式，不从通用清单发明新抽象。
3. 生成可编译的最小骨架后补齐参数检查、失败路径和真实 consumer 接线。
4. 第三方 SDK、寄存器和 legacy C 接口留在其 owning adapter。

## 验证与输出

记录实际命令、结果、未验证项和残余风险；行为变化同步对应文档。需要结构化输出时使用
[`codegen template`](../../templates/codegen-output.md)。
