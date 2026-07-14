# 生成产物说明

> `status`: `supporting`
>
> `scope`: 脚本生成的临时文档与结构化 inventory

本目录承接脚本生成的辅助产物，不保证默认提交。生成结果不是 Capability Contract、完整 module DAG、
运行证据或 Core 准入结论。

当前入口是 [`gen_capability_map.py`](../../scripts/gen_capability_map.py)。它通过正则识别 module、普通
`import` 和特定 `provides/requires_caps` 赋值，不能完整解释 `export import`、CMake 条件、动态 binding
或运行期 provider。生成器报告 capability 数量为 `0` 时会给出 warning，该输出不得称为有效能力图。

从仓库根目录运行 `python scripts/gen_capability_map.py` 可重新生成；实际输出集合以脚本为准。人工维护的
定位入口仍是 [`capability_map.md`](../capability_map.md)。生成文件缺失只表示当前未生成，不代表仓库结构
损坏。
