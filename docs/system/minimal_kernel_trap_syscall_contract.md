# Minimal Kernel Runtime Trap Contract

## 文档状态

- `status`: `supporting`
- `scope`: `kernel.runtime_trap` 的 request、service、policy、result 与 dispatch
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Runtime trap 是 minimal-kernel 的架构无关服务桥，不是 Linux syscall compatibility 或稳定
用户态 ABI。

## 服务与请求

当前服务覆盖 current-task yield/sleep、debug write 和 capability call。Catalog 拥有具体编号、
参数投影和结果名称；未知服务保持 opaque/unsupported，不得回退到其它服务。Typed view 只解释
参数，不改变 request/frame wire 字段或 service 编号。

Request 保存服务、参数、origin 和可选 current task；frame view 还携带平台 capture 得到的执行
现场。Runtime bridge 拒绝 ISR origin，并在需要 current task 时明确报告缺失，不能借用全局或
默认 task。

## Dispatch

Dispatch 先验证 origin/current task，再调用 scheduler-facing runtime 或 policy callback。
Sleep event factory、debug write 和 capability call 可以由 policy 提供；callback 缺失、未知
service 与 runtime 拒绝都不得映射为成功。

## Result

`TrapResult` 统一携带 disposition、error 和 value；只有 `handled + none` 使 `ok()` 为 true。
Runtime bridge 必须区分 `invalid_origin`、`no_current_task`、`invalid_argument` 和
`unsupported_service`。Port/ingress 产生的 unbound、decode 和 writeback 错误不得在 bridge
中被改写。

## 分层

```text
arch frame
  -> RuntimeTrapIngress
  -> TrapFrameView / TrapRequest
  -> RuntimeTrapBridge
  -> runtime service or policy callback
  -> TrapResult
  -> arch writeback
```

- ingress 只负责 frame translation；
- runtime trap 只负责服务语义；
- task syscall 可以复用编号和结果，但不拥有 arch ingress；
- 平台 mapping 不得把 synthetic host frame 冒充真实异常现场。

## 验证入口

Host 与 QEMU 入口由 [`Examples/kernel/README.md`](../../Examples/kernel/README.md) 路由。各入口
只证明自身 fixture，不证明完整用户态隔离或 POSIX ABI。

## 非目标

- 不定义 process、privilege transition 或 user memory validation。
- 不定义 signal、errno、restartable syscall 或 blocking ABI。
- 不把 `capability_call` 自动等同于 Charm Capability Contract 调用。
