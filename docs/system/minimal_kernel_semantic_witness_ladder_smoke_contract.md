# 最小内核 semantic witness ladder smoke 契约（草案）

这份文档描述的是一条本地验证入口，不是新的 schema、exporter、compare brain 或 runtime API。

对应脚本：

- `scripts/semantic_witness_ladder_smoke.ps1`

它把现有 host verifier 已经输出的 semantic witness 行按顺序串起来，确认 message/syscall/session 上半层 witness ladder 仍然能端到端站住。

## 一句话版本

`semantic_witness_ladder_smoke.ps1` 逐个构建并运行现有 host examples，检查每一层已经存在的 `[*-witness] ok=1` 输出。

它只消费现有 host 输出，不生成新事实格式，也不替代 `minimal_kernel_runtime_host_smoke.ps1` 或 runtime evidence bundle。

## 当前 ladder 顺序

当前 v0 ladder 固定覆盖 15 个 host case：

1. `runtime_task_message_syscall_host`
2. `runtime_task_message_syscall_frame_host`
3. `runtime_task_message_syscall_client_host`
4. `runtime_task_message_syscall_pump_host`
5. `runtime_task_message_runtime_service_host`
6. `runtime_task_message_runtime_api_host`
7. `runtime_task_message_syscall_api_host`
8. `runtime_task_message_session_api_host`
9. `runtime_task_message_session_endpoint_host`
10. `runtime_task_message_session_protocol_host`
11. `runtime_task_message_session_dispatch_host`
12. `runtime_task_message_session_acceptor_host`
13. `runtime_task_message_session_service_host`
14. `runtime_task_message_session_service_loop_host`
15. `runtime_task_message_session_roundtrip_host`

这条顺序故意从下层 message-backed syscall witness 往上走到 session roundtrip witness，避免只在最终 roundtrip 成功时倒推中间语义仍然成立。

## 当前检查了什么

脚本当前检查三类事实：

- 每个 host process 必须正常退出。
- 每个 host 的主 demo 行必须包含 `ok=1`。
- 已有 witness 输出行必须包含 expected `standing / collapsed / route=none / handoff` 等稳定标记。

其中 roundtrip 终点额外要求：

- `dispatch=standing`
- `acceptor=standing`
- `protocol=standing`
- `service=standing`
- `pump=standing`
- `completion-corridor=standing`
- `handoff=1`
- `request=1`

这表示最终 seam 没有只靠 completion 值通过，而是同时消费了已有 semantic witness carrier 与 handoff target。

其中 completion corridor 额外要求：

- `open=standing`
- `request=standing`
- `close=standing`
- `ghost=standing`
- `action=1`
- `phase=1`
- `branch=1`
- `identity=1`
- `token=1`
- `payload=1`
- `lifecycle=1`

这表示 client `open / request / close / unsupported open` 四个 completion 的连续性判词，已经从 host example 内联布尔链上抬为源码级 witness carrier。

其中 service-loop case 额外要求：

- `bootstrap=standing`
- `timeout=standing`
- `open_dispatch=standing`
- `open_service=standing`
- `roundtrip=standing`
- `close_dispatch=standing`
- `close_service=standing`
- `ghost_dispatch=standing`
- `ghost_service=standing`
- `ownership-corridor=standing`
- `handoff=1`
- `ownership=1`

这表示 server-side session ownership loop 没有只靠最终 completion 值通过，而是把 bootstrap、timeout、open、request roundtrip、close 与 unsupported open 这些 leg 都收成了 standing witness。

## 构建目录约定

脚本默认使用：

- `D:/Temp/charm-codex/wl`

原因是 C++ module `.pcm` 产物较大，而仓库盘可能空间紧张；同时短路径可以降低 Windows depfile 路径长度风险。

脚本默认：

- 使用 `Release`
- 使用 `--parallel 1`
- 每个 case 前清理 build root
- 结束后清理 build root

这让它适合本地收口验证，但不意味着它已经是 CI 默认入口。

## 当前非目标

这条 ladder smoke 当前不做：

- 不新增 JSON schema
- 不生成 summary sidecar
- 不解析 QEMU 或 raw serial log
- 不新增 compare verdict
- 不替代 runtime evidence bundle
- 不接默认 CI
- 不证明 ARMv7-A lower-half ingress
- 不证明多 session 并发、公平性或完整 RPC framework

## 使用方式

本地运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/semantic_witness_ladder_smoke.ps1 -Clean
```

期望终止行：

```text
[SEMANTIC-WITNESS-LADDER-SMOKE] result=ok cases=15
```

如果需要保留构建目录排查失败，可以加：

```powershell
-KeepBuildRoot
```

## 与其他证据入口的关系

- `minimal_kernel_runtime_host_smoke.ps1` 是更宽的 host runtime smoke。
- `minimal_kernel_runtime_evidence_bundle.ps1` 负责 evidence bundle 汇总。
- `semantic_witness_ladder_smoke.ps1` 只负责把 message/syscall/session semantic witness ladder 的现有 host 输出串成一条本地检查线。

因此它是“witness ladder 回归入口”，不是新的 truth owner。
