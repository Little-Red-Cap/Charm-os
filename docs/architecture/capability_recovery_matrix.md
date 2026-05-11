# 能力回收执行矩阵（草案）

目标：把“能力回收”从口号变成可执行清单，先替换使用，再逐步清理旧实现。

## UI/Ink

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 格式化/输出 | `out.format` / `out.api` | Done | Ink 内部格式化已切换 |
| 日志与诊断 | `out.logger` / `trace_core` | TODO | trace_core 只写入 |
| 容器/池 | `core/service/*` | TODO | 只替换存储模型 |
| span/optional/expected | `core/util/*` | TODO | util 层统一别名 |
| 统计/时间 | `trace_core` / `util.units` | TODO | util.units 禁止时间源 |
| 输入事件队列 | `service_ring_buffer` | Done | ui.queue -> service_ring_buffer |

## UI/Vivid

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 格式化/输出 | `out.format` / `out.api` | TODO | 与 Ink 同步 |
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

## Board / Bringup / System Coordination

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 板级默认输出链 | `ConsoleCaps/BoardCaps -> io.console0 -> out.channel -> out.api/out.logger` | In Progress | H747 已存在 `io.console0` 与 `out.channel` 拼图，但运行期默认路径仍常退回板级直写 console |
| 早期输出例外路径 | `EarlyConsole`（pre-graph / fault / 极早期生存证据） | In Progress | 需要把“合法例外”与“运行期默认路径”明确分开 |
| 统一时间源 | `ClockDesc -> charm.system.clock` | In Progress | H747 已能通过 board landing 注入 clock，但 board app 仍常保留局部时间获取与格式输出习惯 |
| 系统装配 | `CoreSystemChain / BringupConsole / BringupMinimal / init.graph` | In Progress | 拼图已存在，但 board/service/app 默认接法尚未收束成统一路径 |
| 共享诊断 / Shell | `io.registry + io.channel + out.api + service snapshot` | TODO | `system_probe` 类 app 已出现，但 help/status/alive 格式仍大量停留在 app 私有样式 |
| 共享电源 / 板控接入 | `guarded mutation + readable snapshot + named profile` | In Progress | H747 已证明这类 service 需要系统协调层，但当前主要停留在板级经验，尚未沉淀成通约准入样式 |

## FS / ModuleX / Boot

| 能力 | 目标依赖 | 现状 | 备注 |
|---|---|---|---|
| 统一日志 | `out.logger` | TODO | 回收旧输出 |
| 统计/trace | `trace_core` | TODO | 统一 trace 语义 |

---

下一步建议（按优先级）：
1) Board / Bringup：默认输出链 -> `io.console0 -> out.channel -> out.api/out.logger`
2) Board / Bringup：统一 `EarlyConsole` 与 `DefaultConsolePath` 的边界
3) Board / Coordination：共享 shell/status -> `service snapshot`
4) UI/Vivid：格式化/输出 -> out.format
5) UI/Vivid：容器/池 -> core/service 固定容量
6) UI/Ink：日志与诊断 -> out.logger / trace_core

执行规则：每完成一项，必须补一条最小回归（编译 + 行为点）。
