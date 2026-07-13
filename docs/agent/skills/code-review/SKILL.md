# code-review

> status: `supporting`
>
> 用于代码、模块和 PR 审查；不替代 rules、架构评审或对应专题 contract。

## 检查面

优先检查：

- 分层、`init.graph`、IO registry 和执行上下文是否被绕过；
- 协议层 busy-spin、阻塞等待、`io::Channel` 的 `Ok(0)`；
- 动态分配、异常、RTTI、错误模型和时间源是否符合实际路径；
- `signal.emit()`、`state`、`post()` 和长期 connection wiring 是否语义匹配；
- 类型、ownership、生命周期、资源上限和失败后的状态是否可观察。

再判断命名、模块边界、适配层隔离和技术债成本。不要用“常见 C++ 写法”替代仓库规则。

## 执行

1. 读取 diff、源码、CMake、真实 consumer 和相关测试。
2. 使用 [`checklist.md`](checklist.md) 覆盖硬性边界和负例。
3. 按阻断、重要、优化分级；每项给出文件/行号、影响和修复方向。
4. 明确信息不足、未运行的验证和残余风险。

## Escape hatch

遇到绕过规则的临时路径，检查理由、范围、隔离、标记和退出条件；缺少任一项至少记录为重要问题。

输出格式见 [`review template`](../../templates/review-output.md)。
