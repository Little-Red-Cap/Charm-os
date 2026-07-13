# Evidence Script / Schema Surface Governance v0

> `status`: `supporting`
>
> `scope`: `scripts/` 与 `schemas/` 的证据编排、投影和机器格式准入

系统语义的第一解释位置是源码或 contract。Schema 定义公开 artifact shape；producer 定义字段来源；
validator 验证 shape；script 只负责编排、采集、投影和报告。这些表面的存在不证明系统已运行。

## Schema 边界

- Artifact contract schema 声明 identity、root shape 和可依赖字段，不冻结 exporter 的临时结构。
- Projection schema 只保留上游事实、verdict 和 provenance，不回读 raw evidence 或重新选择结果。
- Compare schema 只表达已定义的 baseline/candidate 比较，不扩展 standing、collapsed 或 drift 语义。
- Shared definition 复用 envelope、status/result、artifact ref 与 path，但不得改变现有 JSON wire shape。

## Script 边界

- Entrypoint 处理参数、输出目录、工具编排和 exit code，不复制 exporter/validator 判断。
- Harness 启动 build/test/QEMU/board capture 并保存日志；console 文本不能反向定义系统模型。
- Adapter 将已定义输入投影到既有 schema，不发明 selection、route、compare 或 runtime verdict。
- Library 承载跨工具复用的数据逻辑；PowerShell 优先保留为进程、路径和 CI glue。

## 准入规则

- 新 artifact 不默认新增 `exporter + validator + smoke + inspect + report + compare` 家族。
- 新字段必须有来源、consumer、失败语义和兼容策略，并同步 producer、validator 与 fixture。
- Projection 与 compare 保持分离，只通过 provenance 引用；verdict 不得只靠字段名或脚本隐式定义。
- 先复用既有 envelope、path/ref、status/result 和 shared library，再新增 schema 或 wrapper。
- 失败时保留原始 summary/log 并返回明确 exit code，不用默认值、report 或 fallback 伪造成功。
- `Examples/`、build output、`out/` 和未跟踪实验材料不得成为 schema/source inventory 的输入。
- schema、sample、script、report 的数量与文件存在都不是运行证据或 Core 准入依据。

具体 wire shape、CLI 参数和错误文本由对应 schema、producer、validator 与 runner 维护，不在本文复制。
