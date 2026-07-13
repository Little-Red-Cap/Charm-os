# architect-review

> status: `supporting`
>
> 用于能力归属、分层、装配、IO 路径、错误模型和平台边界评审；不替代
> [`charm-architecture rule`](../../rules/charm-architecture.md) 或 Constitution。

## 输入

- 谁是真实 consumer 和所有者；为什么更小的局部语义不够；
- 入口、依赖、装配和错误边界是否清楚；是否泄漏平台或 provider identity；
- 是否有可重复的正例、反例和跨环境证据；
- 涉及 signal/state 时，同域通知、跨域投递、持久真相和 wiring ownership 是否分开。

## 执行顺序

1. 读取源码、CMake、consumer 和当次验证，不从目录或旧设计稿反推事实。
2. 直接使用 Constitution 六问做准入，不在 skill 中建立替代标准。
3. 比较更小的实现方案，明确推荐方案、不推荐方案和允许的妥协。
4. 给出裁决类别、失败语义、反例和需要更新的唯一契约入口。

## 证据要求

- 相同名称或接口只有在行为、错误、生命周期和 consumer 一致时才算同一语义；
- 各证据域只支持自身覆盖范围；没有 consumer、失败行为或独立证据时保持局部或 exploration。

局部函数或 PR 缺陷切换到 [`code-review skill`](../code-review/SKILL.md)。
