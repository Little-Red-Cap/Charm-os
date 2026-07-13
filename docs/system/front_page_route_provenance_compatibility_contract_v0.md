# Front-page route provenance compatibility v0

## 文档状态

- `status`: `supporting`
- `scope`: `system_compiler.front_page_route/v0` 的旧 provenance 字段读取
- `source`: [`system_compiler_front_page_route_lib.py`](../../scripts/system_compiler_front_page_route_lib.py)

该兼容层只把旧 producer 字段归一化为现行 consumer 字段，不定义第二套路由模型。

## 字段映射

对 `provenance_route_kind != artifact_report_index` 的 entry，consumer 按下表读取；canonical
字段非空时优先：

| canonical | legacy fallback |
|---|---|
| `source_front_page_summary_path` | `source_root_summary_path` |
| `source_front_page_report_markdown_path` | `source_report_markdown_path` |
| `source_front_page_check_text_path` | `source_check_text_path` |

归一化后，validator 只检查 canonical 字段指向的文件存在。`artifact_report_index` 是 discovery
provenance，不是 front-page root，因此三个 canonical 字段保持为空。

## 边界

consumer 可以保留 visited summary 的 `route_provenance`、执行上述字段映射并报告 owner/count。
它不得：

- 解析 host、QEMU、witness 或 compare 原始日志；
- 穿透 producer 已声明的 artifact 边界；
- 合成缺失文件或修改 source summary；
- 新增 schema、route kind、compare verdict 或 traversal edge。

新 producer 应写 canonical 字段。legacy fallback 只保证旧 artifact 可读，不是可扩展的第二套字段族。

## 验证

[`system_compiler_front_page_route_sample_smoke.ps1`](../../scripts/system_compiler_front_page_route_sample_smoke.ps1)
构造 legacy `front_page_route_root` fixture，并验证导出结果包含一个 provenance entry、canonical
路径已填充且文件存在。
