# 文档维护规则

## 文档状态

- `status`: `supporting`
- `scope`: 文档角色、清理判断与验证
- `authority`: [`AGENTS.md`](../AGENTS.md)、[`CONSTITUTION.md`](../CONSTITUTION.md)

操作与信任顺序遵守 `AGENTS.md`；Core 身份与准入遵守 `CONSTITUTION.md`。本文件不建立第三套
权威层级。

## 角色

| 角色 | 责任 |
|---|---|
| README / overview | 范围与下一跳，不复制专题正文 |
| contract | 当前行为、不变量、ownership、失败语义与验证入口 |
| exploration | 未冻结假设、取舍、反例与重新推进条件 |
| tracking | 当前任务、责任与验收，不作为长期规范 |
| archive / reference / generated | 历史、外部对照或生成物，不作为当前事实 |

文件名不授予权威。非 canonical 文档应在前 15 行声明 `supporting`、`exploration`、`archived`
或 `damaged`，并给出 scope 与事实来源。

## 写作

- 一篇文档只承担一种角色；同一主题只保留一个默认入口。
- 先写接口、约束、ownership、失败行为、命令与验证，再写必要背景。
- README 只路由；contract 不写排期、愿景、复盘或阶段旁白。
- 不用“强大、优雅、宏大、真正、显然”等词替代证据。
- 不复制 schema 字段、源码枚举、runner 清单或预期日志；链接到事实源。
- 新文档必须有独立消费方；能补入现有入口或契约时不新增文件。

## 保留与删除

未实施讨论若包含独立假设、反例、失败记录、技术证据、取舍理由或未决约束，压缩为
`exploration` 或移入 archive。

以下内容直接删除：

- 与更高权威文档重复且没有新增事实；
- 只有愿景、情绪、过程旁白、空模板或未来清单；
- 把未实施方案写成当前事实；
- 引用失效且结论无法验证；
- 按对话步骤拆分、离开上下文后不能独立使用。

Git 历史足以追溯低价值中间稿；有独立设计价值的讨论必须在 archive 可发现。

## 工作流

1. 以源码、CMake、真实 target 和当次测试核对事实。
2. 检查入站引用、重复入口、坏链接和文档状态。
3. 将有效事实写回唯一现行入口；将独立讨论归档；删除其余噪声。
4. 同步所有直接引用，不留下 current-to-deleted 路径。
5. 检查 UTF-8/BOM、Markdown 相对链接和 `git diff --check`；按需运行专题门禁。

推荐路径不得把 archive、reference、generated、sample 或 build-only 结果当作当前运行证据。
