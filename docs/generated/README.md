# 生成产物说明

本目录用于承接**脚本生成**的文档与数据，不保证默认提交进仓库。
生成结果是源码 inventory，不是 Capability Contract、完整 module DAG 或 Core 准入结论。

当前与能力图相关的生成脚本为：

- `scripts/gen_capability_map.py`

默认输出目标为：

- `capability_map.generated.md`
- `capability_graph.generated.mmd`
- `capability_data.generated.json`

如果这些文件当前不存在，通常表示它们尚未在本地重新生成，而不是仓库结构出错。

当前生成器通过正则识别 module、普通 `import` 和特定
`provides/requires_caps` 赋值。它不会完整解释 `export import`、CMake 条件、动态 binding
和运行期 provider，因此只能辅助定位。当前源码形状可能导致扫描到 module 但 capability
计数为 `0`；脚本会对此发出 warning，输出不得称为有效能力图。

## 重新生成

在仓库根目录执行：

```powershell
python scripts/gen_capability_map.py
```

生成完成后，可回到以下入口查看：

- 人工维护索引：`docs/capability_map.md`
- 结构化生成产物：本目录下的 `*.generated.*`
