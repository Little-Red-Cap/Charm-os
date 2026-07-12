# 项目规范入口

本目录收纳“开始改代码前最值得先读”的项目规范。

建议顺序：

1. [`项目C++编码要求.md`](项目C++编码要求.md)
   适合先建立语言、运行时、内存与模块使用边界。
2. [`project_conventions.md`](project_conventions.md)
   适合补齐格式化、日志、容器、错误模型与分层约束。
3. [`本项目中C++模块写法要求.md`](本项目中C++模块写法要求.md)
   适合在你已经进入模块实现阶段时再读。

## 怎么用这几篇规范

- 先看 `项目C++编码要求`
  先知道哪些语言与运行时特性在 Charm 里默认允许、受限或禁止。
- 再看 `project_conventions`
  把日志、容器、错误模型、模块分层这些项目习惯补齐。
- 模块风格细节最后看
  避免第一次上手就陷进具体写法细则。

## 使用提醒

- 如果规范文档和架构材料冲突，先按根 `AGENTS.md` 的信任顺序回到 Constitution、核心契约和相关专题源码；`architecture_overview.md` 只提供 supporting 实现地图。
- 代码行为变化、模块入口变化、依赖纪律变化后，应同步更新这里的规范入口。
- 早期完整实践长文保留在
  [`../../archive/project-guidance-and-tracking-v0/`](../../archive/project-guidance-and-tracking-v0/README.md)，
  只用于追溯具体取舍，不覆盖现行规则。
