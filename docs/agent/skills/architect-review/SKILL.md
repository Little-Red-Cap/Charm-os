# architect-review

> status: `supporting`
>
> 用于能力归属、分层、装配、IO 路径、错误模型和平台边界评审；不替代
> [`charm-architecture rule`](../../rules/charm-architecture.md) 或 Constitution。

## 评审问题

必须回答：

- 谁是真实 consumer 和所有者；为什么更小的局部语义不够；
- 入口、依赖、装配和错误边界是否清楚；是否泄漏平台或 provider identity；
- 是否有可重复的正例、反例和跨环境证据；
- 结论应属于 Core、Stable Boundary、Implementation / Tool、Project Fact 还是 Deferred。

涉及事件或状态时补充判断：

- 是同域通知还是跨上下文投递；应使用 `signal.emit()`、`state.set()` 还是 `post()`；
- 是否把事件、状态真相和长期 wiring 混在一起；
- 生命周期和 target 所有权是否可观察。

## 执行顺序

1. 读取源码、CMake、consumer 和当次验证，不从目录或旧设计稿反推事实。
2. 用 Constitution 六问检查跨环境稳定性、必要性、可证明性、平台无关性、例外预算和概念依赖。
3. 比较更小的实现方案，明确推荐方案、不推荐方案和允许的妥协。
4. 给出唯一裁决等级、失败语义、反例和需要更新的源码/契约入口。

## 证据要求

- Host metadata、schema 或 build-only 不能替代 QEMU/real-board 证据；
- 相同名称或接口只有在行为、错误、生命周期和 consumer 一致时才算同一语义；
- 没有真实 consumer、失败行为或独立证据时，保持在局部 implementation 或 exploration。

局部函数或 PR 缺陷切换到 [`code-review skill`](../code-review/SKILL.md)。
