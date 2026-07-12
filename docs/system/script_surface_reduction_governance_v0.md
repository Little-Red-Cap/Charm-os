# Script Surface Reduction / Evidence Harness Governance v0

> **文档状态：`supporting`**

这份文档约束 `scripts/` 的职责，不是脚本实现计划，也不定义 Charm Core。
系统语义的第一解释位置必须是源码、contract、schema 或 shared library；脚本负责编排、采集、校验和输出证据。

旧的数量盘点、pilot 和阶段验收已移入
[`../archive/tool-surface-reduction-v0/`](../archive/tool-surface-reduction-v0/)，不作为当前事实。

## 四层职责

### Entrypoint

- 接收参数、选择输出目录、串接已有工具并返回明确 exit code；
- 只做 CI/人工入口，不复制 exporter、validator 或 compare 判断。

### Harness

- 运行 build、test、QEMU 或 host smoke，并采集日志和 summary；
- 可以知道环境如何启动，但不能把 console 文本反向定义成系统模型。

### Adapter

- 从已定义输入读取事实，输出既有 schema/kind；
- 只做格式投影和路径归一，不发明 selection、route、compare 或 runtime verdict。

### Library

- 承载跨 exporter、validator、compare、report 复用的纯逻辑；
- 复杂数据处理优先进入可测试的 Python/shared library，而不是 PowerShell 巨型函数堆。

## 准入规则

- 新 artifact 不默认新增 `exporter + validator + smoke + inspect + report + compare` 家族；
- compare verdict、selection policy、opening judgment、route/explain/handoff 与 runtime verdict
  不得只存在于脚本；
- PowerShell 优先作为进程、路径和 CI glue，不承担大型 JSON 语义处理或 report engine；
- 文件规模只是风险信号。新增逻辑前必须说明职责、复用边界和失败语义；
- 真实机器证据 harness 优先保持输入、输出和时序稳定，不因整理脚本而顺带改语义。

## 增改前检查

- 现有 exporter、validator、report/check 框架能否复用；
- 新语义是否已经在 contract、schema 或源码中定义；
- 失败时是否保留原始 summary/log，并返回稳定 exit code；
- 重复逻辑是否属于 shared library，而不是另一个 wrapper；
- 新脚本是否确实增加独立证据，而不是只增加一层命名和转发。

schema、report 字段、脚本数量和文件存在都不是运行证据。能力声明必须回到源码与当次 smoke。
