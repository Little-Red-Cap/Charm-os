# code-review

> status: `supporting`
>
> 用于代码、模块和 PR 审查；不替代 rules、架构评审或对应专题 contract。

## 执行

1. 读取 diff、源码、CMake、真实 consumer 和相关测试。
2. 只应用目标路径拥有的专题 contract；使用 [`checklist.md`](checklist.md) 检查 ownership、失败、
   执行上下文、资源和证据。
3. 按严重性给出文件/行号、可观察影响和修复方向。
4. 明确信息不足、未运行验证和残余风险；没有缺陷时也要说明测试缺口。

输出格式见 [`review template`](../../templates/review-output.md)。
