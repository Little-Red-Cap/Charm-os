# 生成产物说明

本目录用于承接**脚本生成**的文档与数据，不保证默认提交进仓库。

当前与能力图相关的生成脚本为：

- `scripts/gen_capability_map.py`

默认输出目标为：

- `capability_map.generated.md`
- `capability_graph.generated.mmd`
- `capability_data.generated.json`

如果这些文件当前不存在，通常表示它们尚未在本地重新生成，而不是仓库结构出错。

## 重新生成

在仓库根目录执行：

```powershell
python scripts/gen_capability_map.py
```

生成完成后，可回到以下入口查看：

- 人工维护索引：`docs/capability_map.md`
- 结构化生成产物：本目录下的 `*.generated.*`
