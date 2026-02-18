# 能力回收执行矩阵（草案）

目标：把“能力回收”从口号变成可执行清单，先替换使用，再逐步清理旧实现。

## UI/Ink

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 格式化/输出 | `out.format` / `out.print` | Done | Ink 内部格式化已切换 |
| 日志与诊断 | `out.logger` / `trace_core` | TODO | trace_core 只写入 |
| 容器/池 | `core/service/*` | TODO | 只替换存储模型 |
| span/optional/expected | `core/util/*` | TODO | util 层统一别名 |
| 统计/时间 | `trace_core` / `util.units` | TODO | util.units 禁止时间源 |
| 输入事件队列 | `service_ring_buffer` | Done | ui.queue -> service_ring_buffer |

## UI/Vivid

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 格式化/输出 | `out.format` / `out.print` | TODO | 与 Ink 同步 |
| 诊断/trace | `trace_core` / `service_trace` | Done | 日志统一接入 out.logger |
| 容器/池 | `core/service/*` | TODO | 固定容量优先 |
| span/optional/expected | `core/util/*` | TODO | util 层统一别名 |
| 资源表/注册表 | `service_fixed_hash_map` / `service_handle_table` | TODO | 统一注册表模型 |

## Audio

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 状态机/事件 | `kernel/EDA` | In Progress | 避免自建状态机 |
| 固定容量容器 | `core/service/*` | Done | player 已静态化 |
| 时间/统计 | `trace_core` | In Progress | trace 统一出口 |
| 文件源 | `fs_vfs` | In Progress | `CHARM_AUDIO_USE_VFS` |

## FS / ModuleX / Boot

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 统一日志 | `out.logger` | TODO | 回收旧输出 |
| 统计/trace | `trace_core` | TODO | 统一 trace 语义 |

---

下一步建议（按优先级）：
1) UI/Vivid：格式化/输出 -> out.format
2) UI/Vivid：容器/池 -> core/service 固定容量
3) UI/Ink：日志与诊断 -> out.logger / trace_core

执行规则：每完成一项，必须补一条最小回归（编译 + 行为点）。
