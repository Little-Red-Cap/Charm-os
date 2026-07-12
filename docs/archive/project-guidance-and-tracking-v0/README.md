# 项目指导与 Tracking 历史归档

状态：archive。

本目录保留早期嵌入式 C++ 实践长文、协作认知和仓库治理 tracking。它们包含可复用的
buffer/DMA、模板约束、escape hatch、任务拆分和 ownership 经验，但也存在以下问题：

- 与 `AGENTS.md`、Agent rules 和较短项目规范重复
- 使用宣言式、劝导式语言代替可检查规则
- “当前”“本周”“进行中”等状态已经过期
- backlog 中混入已完成任务、旧目录和临时粘贴清单

保留文件：

- [`嵌入式C++编程实践指南.md`](嵌入式C++编程实践指南.md)
- [`项目C++编码要求.md`](项目C++编码要求.md)
- [`project_conventions.md`](project_conventions.md)
- [`本项目中C++模块写法要求.md`](本项目中C++模块写法要求.md)
- [`现代C++单片机代码协作认知.md`](现代C++单片机代码协作认知.md)
- [`协作期待与规范.md`](协作期待与规范.md)
- [`主框架全仓审查与收敛_backlog.md`](主框架全仓审查与收敛_backlog.md)
- [`推进TODO与分工.md`](推进TODO与分工.md)
- [`refactor_todo_ownership.md`](refactor_todo_ownership.md)

当前操作规则以根 `AGENTS.md` 和 [`../../agent/README.md`](../../agent/README.md) 为准；项目编码
入口见 [`../../project/standards/README.md`](../../project/standards/README.md)。归档中的任务状态、
责任人、文件路径和完成结论不得作为当前事实。

`本项目中C++模块写法要求.md` 含有无效或非标准示例（如 `import export`、带点 namespace 和
`export "C"`），只保留用于说明旧写法来源，不得复制到代码。
