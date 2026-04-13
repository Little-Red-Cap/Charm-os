# Charm 仓库 Agent 约定

这份文件用于定义本仓库的 agent 协作默认约定。

## 为什么放在这里

- 把最重要的协作共识放在 agent 开工时就能看到的位置。
- 这里保持简短、稳定、直接可执行。
- 详细设计与背景材料继续放在 `docs/` 中维护。

## 默认工作约定

- `git commit` 相关操作优先遵守当前会话策略和用户当次指令。
- 如果当前会话明确允许提交，提交信息使用中文。
- 如果改动是临时性的、探索性的、或尚未完成验证，可以先不提交。
- 如果包含多个逻辑独立的改动，优先分批提交，以保持更清晰的历史记录。
- 当行为、契约、工作流、使用方式发生变化时，按需同步更新相关文档。
- 本环境已配置好 QEMU；在有助于验证运行时行为时，可以按需使用。
- 本仓库本地构建目录统一使用 `cmake-build-*` 命名。
- `cmake-build-*` 已加入 Git 忽略规则，可放心用于临时构建与验证。

## 建议优先阅读的文档入口

- 文档总索引：`docs/README.md`
- 架构总览：`docs/architecture_overview.md`
- Agent 协作入口：`docs/agent/README.md`
- POSIX 总览：`docs/system/posix_support_overview.md`
- POSIX 原则：`docs/system/posix_subsystem_principles.md`

## 作用域说明

- 这份文件默认作用于整个仓库。
- 如果子目录下存在更深层的 `AGENTS.md`，则由更深层文件覆盖对应范围内的约定。
