# Charm Backends

## 文档状态

- `status`: `supporting`
- `scope`: backend contract 与 Host/QEMU/real-board 实现路由
- `authority`: [`contract/backend_contract.md`](contract/backend_contract.md)

`Backends/` 是执行资源域、provider/adapters 和环境证据的归属入口。`Modules/platform/` 中的历史 prototype
可以被临时包装，但不是新 backend 架构真源。

## 目录

| 目录 | ownership |
|---|---|
| [`contract/`](contract/README.md) | capability slices 与 common evidence；Core relation 由 `Modules/core` 拥有 |
| [`host/`](host/README.md) | Host OS/provider integration |
| [`qemu/`](qemu/README.md) | QEMU machine/runtime evidence |
| [`board/`](board/README.md) | real-board BSP/provider evidence |

具体 build leaf 仍位于 `targets/` 或 project target；backend 目录不取代 target/BSP。

## 依赖规则

- app/domain 声明 capability requirement，不依赖具体 backend；
- profile binding 只选择 Provision；Provider Instance 映射留在 project/backend metadata；
- concrete backend 依赖 contract 和自己的 OS/simulator/BSP/HAL adapter；
- Host、QEMU、board implementation 不互相依赖；
- `Modules/system` 不 import concrete backend implementation；
- provider instance、endpoint、transport、HAL handle 和 provider type 都不是 binding target。

术语、evidence、capability slice 和失败边界见 backend contract，不在 README 复制。

## 当前入口

[`host/sdl3`](host/sdl3/README.md) 是当前 SDL-backed Host execution provider，负责 SDL lifecycle、单 event
pump、monotonic clock adapter 和 raster presentation。应用只消费中性 projection，SDL 类型不得泄漏。

验证：

- contract/reference/system provider：[`run-backends-v1-smoke.ps1`](run-backends-v1-smoke.ps1)
- Host SDL vertical smoke：[`host/run-host-sdl3-smoke.ps1`](host/run-host-sdl3-smoke.ps1)
- Host lifecycle/frame/input pressure：[`host/run-host-sdl3-stability-gate.ps1`](host/run-host-sdl3-stability-gate.ps1)

contract gate 当前会为多个独立 fixture 创建 build tree；磁盘受限环境必须先制定 build root/清理策略。
Host、QEMU 与真实板是不同证据域，任一 smoke 不能替代其它环境。
