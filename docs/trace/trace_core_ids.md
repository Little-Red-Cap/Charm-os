# trace_core 事件/计数 ID 清单（草案）

目的：给 trace_core 事件编号提供**统一范围与约束**，避免后续模块相互污染。

## 1. 全局 ID 范围（建议）

| 范围 | 模块域 | 说明 |
| --- | --- | --- |
| 1 - 999 | Kernel | 调度/队列/定时器/同步 |
| 1000 - 1999 | Service | buffer/stream/trace_bus |
| 2000 - 2999 | IO/FS | VFS/Block/FatFs |
| 3000 - 3999 | IO/Shell | facade/cli/core |
| 4000 - 4999 | IO/Input | Raw/Intent/Queue |
| 5000 - 5999 | IO/USB | EP0/CDC/MSC/UAC |
| 6000 - 6999 | Media/Audio | player/sink/decoder |
| 8000 - 8999 | UI/Ink | UI 诊断/输入 |
| 9000 - 9999 | UI/Vivid | GUI 诊断/渲染 |

备注：
- 目前 UI 侧 trace 使用 **局部缓冲**（非全局 trace_core），允许沿用小 ID。
- 若未来统一为全局 trace_core，请按本范围重新映射。

## 2. 已有 ID（本地缓冲）

### UI/Ink（gui.trace）
| ID | 含义 |
| --- | --- |
| 1 | TreeBeginFrame |
| 2 | TreeNodeBegin |
| 100+ | FocusSyncReasonBase（+reason） |
| 200+ | InputIntentBase（+type） |

### UI/Ink（input.trace）
| ID | 含义 |
| --- | --- |
| 1+ | RawButtonBase（+Button） |
| 10 | RawPointerDown |
| 11 | RawPointerMove |
| 12 | RawPointerUp |
| 20 | RawEncoder |

### UI/Vivid（GuiTraceId）
| ID | 含义 |
| --- | --- |
| 1 | FrameNodes |
| 2 | FrameDepthHits |
| 3 | FrameCycleHits |
| 10 | SanitizeRemoved |
| 11 | SanitizeMissing |
| 12 | SanitizeSelf |
| 13 | SanitizeInvalidParent |
| 14 | SanitizeCycle |

## 3. 预留（待补）

Kernel、Service、IO/FS、USB、Audio 的具体 ID 暂未落地，待模块收敛后补齐。
