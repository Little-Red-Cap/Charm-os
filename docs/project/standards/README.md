# 项目规范入口

本目录不再维护一套与 Agent rules 重复的 C++ 规范正文。开始修改代码时按以下顺序读取：

1. 根 [`AGENTS.md`](../../../AGENTS.md)：仓库操作、编码、提交和任务路由。
2. [`embedded-modern-cpp.md`](../../agent/rules/embedded-modern-cpp.md)：语言与抽象选择。
3. [`charm-architecture.md`](../../agent/rules/charm-architecture.md)：分层、IO、初始化和错误模型。
4. 对应任务路由：[`codegen.md`](../../agent/routes/codegen.md) 或
   [`review.md`](../../agent/routes/review.md)。

具体模块还必须读取其目录 README、专题 contract 和源码；通用规则不能替代模块事实。

早期项目 C++ 要求、conventions、模块写法示例和完整实践指南保留在
[`../../archive/project-guidance-and-tracking-v0/`](../../archive/project-guidance-and-tracking-v0/README.md)。
其中存在重复、过期和错误示例，不作为当前编码依据。
