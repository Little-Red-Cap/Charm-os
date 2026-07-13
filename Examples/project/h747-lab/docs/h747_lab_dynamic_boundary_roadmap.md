# H747 Lab Dynamic Image Roles

> status: `supporting`
>
> scope: `dev_loader`、`app_lab` 与 `posix_lab` 的当前职责

行为以源码、CMake target 和当次 smoke 为准；本页只防止三个实验入口被混为同一种 App 模型。

## 角色

| Target | 入口模型 | 当前职责 | 不负责 |
|---|---|---|---|
| `dev_loader` | `charm_app_main(CharmAppApi*, argc, argv)` | packetstream/Store receive、SDRAM stage、QSPI/eMMC media、ELF/ModuleX load、D1 execute region 与 monitor diagnostics | 产品 boot selection、签名、rollback、第二套 App ABI |
| `app_lab` | `charm_app_main(CharmAppApi*, argc, argv)` | embedded `hello_app/player_min` 的最小 ELF loader 与 AppRuntime baseline | download、install、mutable Store policy |
| `posix_lab` | `main(argc, argv, envp)` | POSIX/C ELF 的 spawn/load/wait、fd/path/pipe/stat compatibility | `CharmAppApi` 或 resident App entry |

`dev_loader` 是 resident development mainline；`app_lab` 是同一 App ABI 的 embedded-image baseline；
`posix_lab` 保持独立 POSIX entry，不做隐式 ABI 转换。

## 共享边界

```text
received bytes or Store entry
-> AppImage
-> ELF/ModuleX loader
-> AppRuntime
-> charm_app_main(CharmAppApi*, argc, argv)
```

- received、QSPI 与 eMMC 在 loader 前收敛，transport/media identity 不进入 App ABI。
- ELF 是主 image format；ModuleX 是第二 format，不是第二 App model。
- Host/QEMU 验证 loader/runtime 语义；H747 验证真实 memory、cache、transport 和 media。
- App sample 只消费 capability boundary，不拥有平台或外设 policy。

## 验证

Resident App 改动先验证 artifact/host semantics，再按影响范围选择 QEMU、H747 focused smoke 或完整
QSPI/eMMC matrix。`app_lab` 与 `posix_lab` 仅在自身 baseline 受影响时复测，不是每次
`dev_loader` 迭代的固定前置步骤。

## 入口

- [`dev_loader`](../apps/dev_loader/README.md)
- [`app_lab`](../apps/app_lab/README.md)
- [`posix_lab`](../apps/posix_lab/README.md)
- [`H747 layering`](h747_lab_layering_contract.md)
