# Host Backends

## 文档状态

- `status`: `supporting`
- `scope`: Host backend ownership、实现与验证路由
- `authority`: [`../contract/backend_contract.md`](../contract/backend_contract.md)

Host backend 在 PC OS 上快速验证 Charm 的 runtime 与 capability 语义，不声称模拟真实启动状态、硬件
时序、IRQ controller、pinmux 或 reset 行为。

## Ownership

Host backend 拥有 OS/provider integration、provider instance 和本环境证据。应用只依赖 capability；OS
API、file handle、SDL 类型、adapter 和 endpoint 不得成为 profile binding target。

## 当前实现

- [`host_reference.hpp`](host_reference.hpp)：memory-backed console 与 block provider 候选，只供 contract
  smoke 使用，不是公开 `Modules/` API 或 service locator。
- [`sdl3/`](sdl3/README.md)：SDL lifecycle、单 event pump、monotonic clock 与 raster presentation provider；
  不拥有 Player command、UI lifecycle、storage、audio、font 或 product profile。

`Modules/platform/win` 等历史 prototype 不是 Host contract 真源。

## 验证

- reference 与共同 capability smoke：[`../run-backends-v1-smoke.ps1`](../run-backends-v1-smoke.ps1)
- SDL vertical smoke：[`run-host-sdl3-smoke.ps1`](run-host-sdl3-smoke.ps1)
- SDL lifecycle/frame/input pressure：[`run-host-sdl3-stability-gate.ps1`](run-host-sdl3-stability-gate.ps1)

SDL 脚本的 dependency provenance、参数和实际覆盖以脚本及 [`sdl3/README.md`](sdl3/README.md) 为准。
磁盘受限时显式传入同一个 `-BuildDir`，不要使用会生成多个默认 build tree 的组合 gate。
