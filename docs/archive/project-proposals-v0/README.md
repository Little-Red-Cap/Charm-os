# Project Proposals v0 归档

> status: `archived`
>
> 本目录保存早期项目/构建提案中仍有独立取舍价值的部分，不作为当前实现或契约。

当前判断从 [`Charm Core Contract`](../../architecture/charm_core_contract.md) 和
[`project/README.md`](../../project/README.md) 进入。Player 使用的工程对象状态桥接见
[`charm_工程对象模型草案.md`](../../project/charm_工程对象模型草案.md)。

## 保留内容

| 文件 | 独立价值 |
|---|---|
| [`build_model_retained_notes.md`](build_model_retained_notes.md) | explicit target、BSP source ownership、preset/workflow 与迁移失败边界 |
| [`early_diagnostics_retained_notes.md`](early_diagnostics_retained_notes.md) | early/full sink、startup 与复杂装配前诊断 |
| [`usb_declarative_retained_notes.md`](usb_declarative_retained_notes.md) | USB spec/runtime binding、generator、MSC storage 与调试边界 |
| [`config_module_draft.md`](config_module_draft.md) | typed config、生成实现和 compatibility alias 的分层问题 |

重新推进任一提案前，先核对当前 consumer、源码/CMake、失败语义和跨环境证据；优先补入现有契约
或建立局部实验，不恢复整套旧词汇。
