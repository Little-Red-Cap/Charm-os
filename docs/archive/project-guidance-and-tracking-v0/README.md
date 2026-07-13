# 项目指导与 Tracking 历史归档

状态：archive。

本目录保留早期嵌入式 C++ 取舍笔记和仓库 review。它们包含可复用的 buffer/DMA、模板约束、
escape hatch 和具体审查发现，但也存在以下问题：

- 与 `AGENTS.md`、Agent rules 和较短项目规范重复
- 使用宣言式、劝导式语言代替可检查规则
- “当前”“本周”“进行中”等状态已经过期
- backlog 中混入已完成任务、旧目录和临时粘贴清单

保留文件：

- [`embedded_cpp_retained_notes.md`](embedded_cpp_retained_notes.md)：从早期 593 行实践指南中提取的
  执行上下文、DMA、模板构造、错误边界和 escape hatch 取舍。
- [`repository_review_retained_notes.md`](repository_review_retained_notes.md)：从旧全仓 backlog 提取的
  聚合入口、复合职责、桥接边界和分刀顺序。

当前操作规则以根 `AGENTS.md` 和 [`../../agent/README.md`](../../agent/README.md) 为准；项目编码
入口见 [`../../project/standards/README.md`](../../project/standards/README.md)。归档中的任务状态、
责任人、文件路径和完成结论不得作为当前事实。

仍可复用的协作判断是：接口握手稳定后再拆并行轨道；共享高风险文件采用短时认领；接口、迁移和
清理分开提交。
