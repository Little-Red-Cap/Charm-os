# Board Backends and BSPs

## 文档状态

- `status`: `supporting`
- `scope`: real-board backend、BSP ownership 与证据路由
- `authority`: [`../contract/backend_contract.md`](../contract/backend_contract.md)

Board backend 把 capability provider 绑定到真实硬件；BSP 拥有 board facts 和硬件接线，但不得重定义
共同 backend contract。profile binding 选择 provider instance，不选择 UART route、DMA channel、HAL handle、
pinmux fact 或 endpoint。

## BSP responsibilities

- SoC and board identity.
- Clock sources and relevant clock tree facts.
- Pinmux routes.
- Early console route.
- IRQ controller and interrupt line facts.
- Memory regions.
- Startup and linker-script inputs.
- Vendor SDK or HAL binding.
- Board evidence such as register readback, probe results, and bring-up logs
  presented as structured facts.

HAL/vendor SDK 可以留在 BSP 内，但不是 Host、QEMU 与 board 共享的 Charm backend 语言。

## Reference evidence

[`board_reference.hpp`](board_reference.hpp) 只验证 `BackendEvidenceView` 能表达 provider 与 board facts；它不
驱动硬件、不包含 HAL，也不证明 H747 ready。未知 clock/IRQ fact 必须保持 unknown/missing，不能把 partial
bring-up 写成 readiness。

验证由 [`../run-backends-v1-smoke.ps1`](../run-backends-v1-smoke.ps1) 编排。磁盘受限环境不要在
`reference_smoke/` 内创建独立 build tree。

## Relationship to targets

`targets/` 仍拥有 concrete build leaf。target 可以选择 BSP，但 target、SoC、board 与 backend domain 不是
同一身份；目录迁移不得把这些角色压成一个概念。
