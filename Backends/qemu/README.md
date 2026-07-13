# QEMU Backends

## 文档状态

- `status`: `supporting`
- `scope`: QEMU backend ownership、reference evidence 与运行验证路由
- `authority`: [`../contract/backend_contract.md`](../contract/backend_contract.md)

QEMU backend 在 machine model 中验证 startup、trap/exception、timer、interrupt routing、early console 与
memory map 假设，位于 Host 语义验证和 real-board 硬件证据之间。它不替代真实外设验证。

## Ownership

QEMU backend 拥有 machine/provider integration、provider instance 和本环境证据。machine detail、trap
vector、adapter、HAL-like stub 与 endpoint 不能成为 app/domain 依赖或 profile binding target。

## 当前实现

[`qemu_reference.hpp`](qemu_reference.hpp) 是 header-only evidence candidate，导出 early console provider、
binding evidence 以及 timer/trap/IRQ/memory-map facts。其 resident ELF region 是
`0x20080000..0x20090000`。

reference 不启动 QEMU，也不模拟 H747 的 DSI/LTDC/eMMC、touch、USB CDC、FMC SDRAM 或 board HAL。

## 验证

- reference contract smoke：[`../run-backends-v1-smoke.ps1`](../run-backends-v1-smoke.ps1)
- resident ELF QEMU runtime：[`../../Examples/system/resident_elf_qemu_smoke/README.md`](../../Examples/system/resident_elf_qemu_smoke/README.md)

两类证据不可互相替代，更不能替代 real-board capture。
