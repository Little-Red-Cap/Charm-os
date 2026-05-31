# Resident Image Platform v1 契约

本文固定 Charm 当前动态 App 平台的第一代架构边界。目标不是定义产品级
bootloader，而是把已经跑通的下载、外部存储、装载、执行路径收成同一条平台主链。

## 主链

Charm 的动态 App 必须汇入这条链路：

```text
PlatformBoot
  -> ResidentRuntime
  -> ImageSource/ImageStore
  -> AppImage/ProgramImage
  -> Loader
  -> RuntimeDomain
  -> AppRuntime
  -> Capability Table
```

含义如下：

- `PlatformBoot` 只负责把一个 resident runtime 带起来；它不直接承担每个 App 的装载策略。
- `ResidentRuntime` 是常驻平台运行时，例如 H747 `dev_loader` 或未来产品 runtime。
- `ImageSource/ImageStore` 只产出 image bytes 或 named image，不定义 App ABI。
- `AppImage/ProgramImage` 是 loader 可消费的 image carrier，不代表文件系统或进程模型。
- `Loader` 负责把 image format 解释成 `LoadedAppImage`，入口仍是 `charm_app_main`。
- `RuntimeDomain` 描述执行域，例如 host、H747 CM7、未来 CM4/remote domain；它不是普通线程别名。
- `AppRuntime` 负责 `lookup/load/abi/argv/start/exit` 主语义和诊断。
- `Capability Table` 是 App 可见的唯一平台能力边界。

## ImageSource / ImageStore 边界

USB CDC、UART raw packetstream、QSPI NOR、eMMC、未来网络或文件系统入口，都不能各自定义 App 模型。
它们只能收敛成：

```text
received bytes or store byte range -> AppImage -> staged AppImageSource -> AppRuntime
```

当前 Store v1 是 `header + entries + payload` 的 named byte-range store。它不是 filesystem、
package manager、manifest DSL、slot manager、签名系统或回滚策略。

ModuleX 是第二 image format，不是第二 App model。ModuleX loader 后续必须输出同一种
`LoadedAppImage`，入口仍解释为：

```c
extern "C" int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

## RuntimeDomain 边界

`RuntimeDomain` 用来表达代码最终在哪个执行域运行：

- `host`：host-only smoke 或桌面开发后端。
- `h747_cm7`：当前 H747 resident runtime 的主执行域。
- `h747_cm4`：未来可能的协处理执行域。
- `future_remote`：未来远端核、Linux core、外部处理域或代理域。

多核异构不能被伪装成普通线程。跨核执行需要显式 domain、image handoff、内存归属、
cache/一致性策略和 capability proxy；这些属于平台装配层，不进入 App ABI。

## Stage / Load Arena 边界

receive cache、stage cache、ELF execute region 是平台资源，不是 App ABI 的一部分。

H747 当前形态为：

- received payload 可落在 SDRAM receive arena。
- named image staging 可落在 SDRAM stage cache。
- App ELF execute region 由 resident runtime 明确描述，例如 D1 RAM `0x24070000..0x24080000`。

App 不应该感知这些 arena 的地址、cache 维护或外设内存归属。loader/runtime backend
负责把 image bytes 放到可执行位置，并在进入 `charm_app_main` 前完成必要 cache 处理。

## Capability Backend / Proxy 边界

App 只能看见 `CharmAppApi` capability table。真实能力可以由本地 backend 提供，也可以由
跨核、远端 runtime 或操作系统代理提供，但 App ABI 不因此改变。

因此：

- display/input/storage/AFE 可以从 stub 后端逐步替换为真实后端。
- 跨核 backend 必须表现为 capability proxy，不让 App 直接依赖 mailbox、RPC 或外设所有权。
- C++ class、vtable、exception、RTTI 不跨 App ABI 边界。

## 诊断契约

动态 App 主链的失败必须落到稳定阶段：

- `lookup`：image/source/store name 不存在或 source 无效。
- `load`：loader 不支持格式、ELF/ModuleX 装载失败、backend 错误。
- `abi`：入口缺失或 `CharmAppApi` magic/version/size 不匹配。
- `argv`：名称或参数构造失败。
- `start`：准备调用入口。
- `exit`：入口返回并回收 exit code。

transport、store、arena、loader 可以保留各自 backend error，但不能绕开这些阶段另造第二套
App 执行诊断。

## 当前非目标

- 不把 `dev_loader` 定义为产品 bootloader。
- 不引入 filesystem、manifest、slot/signature/rollback 策略。
- 不实现 M4 mailbox、跨核 RPC 或多核调度。
- 不把 prototype 提升到 `Modules/*`。
- 不修改 `CharmAppApi v1`、Store v1、packet v0 或现有 H747 monitor 命令。
- 不参与 Player UI 或真实 display/touch/input 后端设计。
