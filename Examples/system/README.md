# System 示例入口

本目录收纳 system 装配、device model、runtime slot export、AppHost 与 power 相关样例。

如果你还没先看系统侧文档，建议先回到：

- [`../../docs/system/README.md`](../../docs/system/README.md)
- [`../../docs/architecture/driver_model.md`](../../docs/architecture/driver_model.md)

## 按任务进入

### 我想回归 RTE Phase 0 语义基线

运行：

```powershell
.\run-rte-phase0-smoke.ps1
```

这个脚本会配置、构建并运行路线图 Phase 0 的 RTE / Spine host-only smoke。
它只验证语义证据链，不修改 H747-lab、不烧录、不提升公共 RTE API。

### 我想回归 `app_lab` 的 host-only 主链

先读：

- [`app_lab_mainline_smoke/README.md`](app_lab_mainline_smoke/README.md)

这个示例把 `app_lab` 当前的官方主叙事收成一条单独 host-only smoke：
embedded app -> QSPI install -> named/raw run-path -> generic file-backed stub
-> diagnostics-first status。它不是通用 `AppRuntime` proof，也不触碰 H747 板测。

### 我想看 AppHost / poster / deferred signal

先读：

- [`app_host_poster_demo/README.md`](app_host_poster_demo/README.md)

### 我想看 Charm Spine 最小平台形态

先读：

- [`charm_spine_smoke/README.md`](charm_spine_smoke/README.md)

这个示例验证 `Capability -> Component -> Profile -> Projection -> Evidence`
这一条 Charm Spine v0 主链路的最小可执行形态。

### 我想看 Charm Spine evidence side-channel 投影

先读：

- [`charm_spine_evidence_projection_smoke/README.md`](charm_spine_evidence_projection_smoke/README.md)

这个示例验证 evidence 是从同一份 component/profile 结构派生出的只读 side-channel
projection，不属于 init、runtime 或 provider log 路径。

### 我想看 reflected profile 如何贯穿 Charm Spine 主链

先读：

- [`charm_spine_reflected_profile_smoke/README.md`](charm_spine_reflected_profile_smoke/README.md)

这个示例验证 reflected spec 可以作为源事实参与 `profile resolution`，再继续投影到
`init.graph`、app `ContextView` 与只读 evidence side-channel，形成一条更完整的
`reflected spec -> profile -> projection -> evidence` 证据链。

### 我想看 RTE 能力装配语义

先读：

- [`rte_component_context_smoke/README.md`](rte_component_context_smoke/README.md)

这个示例验证 `ComponentDesc + explicit binding + ContextView + EvidenceFrame`
能否先在普通 C++ host smoke 中成立，不提升为公共模块。

### 我想看 RTE component topology 如何投影到 init.graph

先读：

- [`rte_init_projection_smoke/README.md`](rte_init_projection_smoke/README.md)

这个示例验证 `ComponentDesc -> init::Node / init::Graph` 的最小投影桥，
保持 RTE 只是 capability composition boundary，不接管 runtime 或调度。

### 我想看 RTE profile 如何同时 materialize init 与 ContextView

先读：

- [`rte_profile_materialization_smoke/README.md`](rte_profile_materialization_smoke/README.md)

这个示例验证同一份 component/profile 语义可以同时投影为 `init.graph`
和 app 可见的 `ContextView`，并保持 provider binding 显式消歧。

### 我想看 RTE profile resolution 如何判定装配是否合法

先读：

- [`rte_profile_resolution_smoke/README.md`](rte_profile_resolution_smoke/README.md)

这个示例验证 profile 不是 provider 集合或运行期 lookup table，而是必须自证
requirements、providers 与 bindings 一致的显式装配结论。

### 我想看同一个 app 如何在 host/board profile 间切换

先读：

- [`rte_profile_selection_smoke/README.md`](rte_profile_selection_smoke/README.md)

这个示例验证同一份 app requirements 可以在 host/mock 与 board/H747 provider
之间切换，app 语义保持不变，provider identity 只体现在 profile/evidence 投影里。

### 我想看一个 provider 如何显式承担多个 role

先读：

- [`rte_multi_role_provider_smoke/README.md`](rte_multi_role_provider_smoke/README.md)

这个示例验证同一个 provider 可以同时承担多个 role，但每个 role 都必须有独立的
`Provided` token 与显式 binding，不能靠 capability kind 隐式复用。

### 我想看 RTE projection 如何被 profile resolution 门禁保护

先读：

- [`rte_projection_gate_smoke/README.md`](rte_projection_gate_smoke/README.md)

这个示例验证 init、context 与 evidence projection 都只能从已解析的
`ResolvedProfile` 派生，不能绕过 profile resolution 直接吃原始装配事实。

### 我想看 RTE projection 是否保持 provider identity 一致

先读：

- [`rte_projection_consistency_smoke/README.md`](rte_projection_consistency_smoke/README.md)

这个示例验证同一个 resolved profile 派生出的 init、context 与 evidence projection
必须保留同一份 binding 的 provider identity，不能各自偷偷换 provider。

### 我想看 RTE explain/report 是否只是只读投影

先读：

- [`rte_explain_projection_smoke/README.md`](rte_explain_projection_smoke/README.md)

这个示例验证 explain/report surface 只能从已解析的 `ResolvedProfile` 派生，
用于解释 host/H747 profile 的 capability binding 与 provider facts 差异；
它不读取 runtime provider 实例，也不是 artifact JSON、DSL 或 system compiler 平台化入口。

### 我想看 RTE ContextView 如何按组件裁剪世界

先读：

- [`rte_context_slice_smoke/README.md`](rte_context_slice_smoke/README.md)

这个示例验证同一个 resolved profile 可以包含多个 app 与 provider，但每个 app
只能得到按自身 requirements 裁剪后的 `ContextView`，不会退化成全局 world。

### 我想看 RTE evidence 如何独立于 ContextView slice

先读：

- [`rte_evidence_slice_smoke/README.md`](rte_evidence_slice_smoke/README.md)

这个示例验证 evidence 是 profile-wide side channel：provider 可以被统一观测，
但 evidence collector 不进入任何 app 的 `ContextView`。

### 我想看 C++ static reflection probe

先读：

- [`static_reflection_probe/README.md`](static_reflection_probe/README.md)

这个示例只做编译期 smoke，用来确认最新 `arm-none-eabi-g++`
已经具备 `__cpp_impl_reflection`、`<meta>`、`^^T` 和 `[:R:]`。它用反射值描述
最小 RTE `Requirement/Provided` kind/role 语义，并验证 spec 字段形状可被成员反射发现。

### 我想看 reflected spec 如何投影到 ContextView / evidence

先读：

- [`rte_reflected_context_smoke/README.md`](rte_reflected_context_smoke/README.md)

这个示例把 static reflection proof 和 RTE context proof 接起来：反射得到的 component spec
作为源事实，继续 materialize 到 `ComponentDesc + ContextView + EvidenceFrame` 语义。

### 我想看 reflected spec 如何参与 profile resolution 门禁

先读：

- [`rte_reflected_profile_resolution_smoke/README.md`](rte_reflected_profile_resolution_smoke/README.md)

这个示例把 static reflection 源事实接到 RTE profile resolution 规则：`Requirement/Provided`
使用反射 token 描述来源，profile 必须在编译期证明 binding、provider 与 role/capability 匹配，
成功后才 materialize 最小 `ContextView`。

### 我想看 runtime device 如何收口成稳定 capability

先读：

- [`device_runtime_block_slot_demo/README.md`](device_runtime_block_slot_demo/README.md)
- [`device_runtime_channel_slot_demo/README.md`](device_runtime_channel_slot_demo/README.md)

### 我想看 device bus / registry 的最小闭环

先看：

- [`device_bus_demo/`](device_bus_demo/)
- [`device_registry_demo/`](device_registry_demo/)

### 我想看 power 策略与 trace

先看：

- [`power_demo/`](power_demo/)

## 使用提醒

- 这里偏 system 能力验证，不要和 bootloader 主线或板级 bring-up 文档混成一层。
- 如果你在看启动链路，请回到 [`../boot/README.md`](../boot/README.md) 与 [`../../docs/boot/README.md`](../../docs/boot/README.md)。
