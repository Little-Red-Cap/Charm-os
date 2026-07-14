# IO 分层边界

> status: supporting
>
> 本文解释 IO primitive 的依赖位置。Channel、Reactor、Registry 的行为分别由
> 本目录三份 `*_contract.md` 约束。

## 三个 Primitive

| Primitive | 负责 | 不负责 |
|---|---|---|
| `io::Channel` | 非阻塞 byte read/write/flush | 等待、重试、协议和生命周期 |
| `io::Reactor` | 事件入队、waker、task-context callback dispatch | 在 ISR 执行 callback、阻塞调度 |
| `io::Registry` | 固定容量 endpoint 发布与查找 | 对象所有权、热插拔生命周期、动态分配 |

协议和 service 在这些 primitive 上构建。调用结果、callback budget、name backing lifetime 与
registry ownership 由对应契约定义，本页不重复。

## 实现层次

```text
platform/controller backend
    -> io HAL or raw endpoint
    -> Channel / block device / protocol adapter
    -> registry or stable slot export
    -> service / domain consumer
```

- Platform/backend 拥有寄存器、IRQ、DMA、clock、pinmux 和板级内存布局。
- IO HAL 只表达控制器或原始设备能力，不包含 UI、audio、filesystem 等领域策略。
- Protocol/adapter 拥有 framing、状态机、buffer 和错误映射，但不得 busy-spin。
- Service 拥有可被应用消费的运行时语义。
- Domain 消费 capability；不直接依赖 `BoardCaps`、match score 或 driver context。

`io/driver` 中的控制器适配和 `system/device` 中用于 runtime discovery 的
`device::Driver` 不是同一种生命周期模型，不能只因都叫 driver 就合并。

## 两种装配路径

板级静态资源：

```text
BoardCaps -> init.graph binding -> io/block registry -> consumer
```

运行时发现资源：

```text
Bus -> DeviceDesc -> RuntimeDriver -> stable slot -> registry -> consumer
```

静态资源不需要伪装成热插拔设备；运行时设备也不能把短生命周期裸指针直接发布
给长期消费者。detach 后仍需保留入口时，使用 channel/block stable slot，使已有
handle 返回 `noent`，再由明确的 unexport 移除名称。

## 依赖边界

- Channel、Reactor 与 Registry 的调用、容量、错误和生命周期分别由
  [`io_channel_contract.md`](io_channel_contract.md)、[`io_reactor_contract.md`](io_reactor_contract.md)
  和 [`io_registry_contract.md`](io_registry_contract.md) 定义。
- HAL、backend 和 protocol 不依赖 domain 语义；输入采样只产生 raw event，UI intent、gesture 和控件
  策略不进入 HAL。
- filesystem、USB、network 和 input 可以复用 IO primitive，但不能反向改变其契约。

Driver/discovery 的完整边界见
[`../architecture/driver_model.md`](../architecture/driver_model.md)。历史 VSF 对照只从
[`../reference/vsf/README.md`](../reference/vsf/README.md) 进入，不作为当前规则。
