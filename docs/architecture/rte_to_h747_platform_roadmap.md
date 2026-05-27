# Charm RTE 到 H747 平台化路线图

本文定义 Charm 近期最重要的平台化主线：

```text
RTE capability composition boundary
  -> H747 Lab real-board pressure test
  -> host/board shared app semantics
  -> system compiler explainable artifact
```

它不是新 DSL、manifest、generator、runtime framework 或公共模块提升计划。
它的目标是把 `Capability -> Component -> Profile -> Projection -> Evidence`
这条 Charm Spine 主链推进到真实 H747 板级压力场，并保持应用语义不被平台偶然性定义。

## 1. 总目标

Charm 的长期目标是让嵌入式软件作为“系统”存在，而不是作为某个平台、
HAL、启动文件或工程模板的附庸存在。

对当前阶段来说，这句话落到工程上就是：

- app / domain 代码只依赖 capability 语义。
- host/mock 和 H747 只通过 profile / binding / board package 差异切换。
- init、context、evidence 都从同一份已解析 profile 派生。
- evidence 以结构化事实解释系统，而不是把日志当成证据。
- 工具化、反射和 ABI 边界必须晚于普通 C++ 语义验证。

RTE 在这条路线中的定位固定为：

```text
capability composition boundary
```

它不接管调度，不实现事件循环，不充当 service locator 或 DI container，
也不试图复制 AUTOSAR RTE。

## 2. 当前主线选择

近期主线选择：

```text
RTE -> H747
```

第一条真实垂直切片选择：

```text
Display + Player
```

原因：

- 它最直接证明同一份 app / UI 语义可以在 host/mock 与 H747 provider 间切换。
- 它能减少上层开发对同一块真实板的烧录争抢。
- 它同时压测 display、input、clock、log、profile binding 与 evidence side-channel。
- 它比先抽公共模块更安全，因为真实板级后端会先拷打语义。

## 3. 阶段路线

### Phase 0：守住当前语义证据链

目标：继续把 RTE / Spine 语义压在 host-only smoke 中，不急着公共模块化。

当前回归基线：

- `rte_component_context_smoke`
- `rte_init_projection_smoke`
- `rte_profile_materialization_smoke`
- `rte_profile_resolution_smoke`
- `rte_projection_gate_smoke`
- `rte_projection_consistency_smoke`
- `rte_explain_projection_smoke`
- `rte_context_slice_smoke`
- `rte_evidence_slice_smoke`
- `rte_multi_role_provider_smoke`
- `rte_profile_selection_smoke`
- `charm_spine_smoke`
- `charm_spine_evidence_projection_smoke`

阶段验收：

- 上述 smoke 保持通过。
- RTE prototype 仍留在 smoke / H747-lab 试验层。
- explain / report surface 只能作为已解析 profile 的只读 projection，不作为
  system compiler artifact 管线或新平台入口。
- 不新增 manifest、DSL、generator。
- 不把 RTE 做成 runtime framework。

### Phase 1：Display + Player 垂直切片

目标：把 host/mock 与 H747 的第一条真实 app 语义切通。

实施方向：

- 以 `Examples/project/h747-lab` 为真实压力场。
- 以 `RasterDisplayWorld` / `RasterDisplayInputWorld` 作为当前 source-level `ContextView` 形态。
- Player / app domain 只依赖 capability concept，不依赖 HAL、DSI、LTDC、I2C、TIM 或 BSP global。
- host profile 绑定 host framebuffer、mock input、mock clock、mock log。
- H747 profile 绑定 raster framebuffer、DSI/LTDC display service、input service、console、clock。
- app 代码不因 host / H747 profile 切换而变化。
- provider identity 只进入 profile binding 与 evidence projection，不泄漏进 app。

阶段验收：

- `h747_lab_host_player` 可在 PC 上验证 Player / app 行为。
- `h747_lab_player` 或 `h747_lab_player_md3` 可在 H747 上使用同一 app / domain 语义。
- host / H747 profile 的差异可被 evidence 解释。
- H747 临时路径不被上升为 Charm 公共契约。

### Phase 2：RTE prototype 准公共化候选

目标：在 host smoke 与 H747 垂直切片都成立后，再选择最小公共模块候选。

允许提升的前提：

- 同一概念在 host smoke 与 H747 真实 target 中都被验证。
- 命名已经稳定。
- 不依赖 H747 私有服务。
- 不需要反向修改大量 app 代码才能成立。
- 能解释未来 ABI capability table 的映射方向。

候选类型：

- `Requirement<Kind, Role>`
- `Provided<Kind, Role>`
- `ProviderDesc`
- `ComponentDesc`
- `ProfileBinding`
- `ResolvedProfile`
- `ContextView`
- `EvidenceFrame`

阶段验收：

- 只提升最小稳定子集。
- 不一次性搬运所有 smoke prototype。
- 公共模块不引入 H747 私有假设。

### Phase 3：静态反射作为样板压缩器

目标：把静态反射放在第二阶段辅助位置，不作为 Phase 1 阻塞项。

使用原则：

- 先证明普通 C++ constexpr 模型成立。
- 反射只用于减少重复声明、生成字段枚举、辅助 profile / spec 检查。
- 反射不改变 RTE 语义。
- arm-none-eabi 工具链稳定前，不让反射进入 H747 必需路径。

阶段验收：

- reflected smoke 继续存在，但不阻塞 H747 Display + Player 切片。
- 反射发现出来的结构必须能映射回手写 `Component / Profile` 语义。

### Phase 4：System Compiler / Artifact Report 收束

目标：把 RTE / H747 形成的系统结构继续投影为可解释、可比较、可审计的结果物。

实施方向：

- 将 H747 Display + Player 的 profile / binding / evidence 纳入 system compiler vocabulary。
- 输出最小 artifact report，覆盖 capabilities、binding result、bringup order、evidence summary、resource facts。
- explain surface 先只读，不做交互式大平台。
- 资源契约先报告，不立即全仓库硬门禁。

阶段验收：

- 能回答 host profile 与 H747 profile 的 binding 差异。
- 能回答 Player 为什么不依赖 DSI / LTDC。
- 能回答当前 H747 display / input evidence 来自哪个 provider。

## 4. 设计红线

- RTE 只做能力装配边界，不接管运行时。
- Component topology 是源事实，init / context / evidence 都是 projection。
- `ContextView` 是裁剪后的 app 世界，不是 global world。
- Profile 是装配结论，不是 CMake preset 的同义词。
- BoardPackage 承载板级事实，不偷偷定义系统秩序。
- Evidence 是结构化事实，不是 log；presentation 单独格式化。
- Host、board、ABI 是不同载体，不混成一种边界。
- 公共 API 提升必须等 host + H747 两端验证后再做。

## 5. 验证纪律

每次 RTE 语义改动必须回归 `Examples/system` 现有 RTE / Spine smoke。

每次 H747 app / world 改动必须至少回归：

```powershell
cmake -S Examples/project/h747-lab/host -B Examples/project/h747-lab/cmake-build-host-debug
cmake --build Examples/project/h747-lab/cmake-build-host-debug
ctest --test-dir Examples/project/h747-lab/cmake-build-host-debug -C Debug --output-on-failure
```

H747 固件侧阶段验收至少覆盖：

- `h747_lab_display_raster_demo`
- `h747_lab_player` 或 `h747_lab_player_md3`

Display + Player 切片必须证明：

- host / H747 profile 下 app 输出语义一致。
- provider identity 不泄漏进 app。
- explain / report 只解释 binding 与 fact 差异，不进入 app runtime。
- evidence 不直接格式化日志。
- evidence collector 不进入 app `ContextView`。

## 6. 与相关文档的关系

- `charm_methodology_charter.md`
  定义 Charm 为什么要让系统秩序独立于平台偶然性。
- `charm_spine_v0.md`
  定义 `Capability -> Component -> Profile -> Projection -> Evidence` 主脊梁。
- `rte_capability_composition_contract_v0.md`
  定义 RTE 作为 capability composition boundary 的 v0 契约。
- `system_compiler_roadmap.md`
  定义系统可编译、可举证、可审计、可托管的中长期主轴。
- `Examples/project/h747-lab/docs/h747_lab_capability_contract.md`
  定义 H747-lab 当前 source-level capability contracts。

本文只负责把这些文档收束成近期平台化路线，不替代任何一个具体契约。

如果当前问题已经进入“哪些 Spine/RTE 语义应该真正迁到 H747，哪些只保留在
host proof，哪些不能直接变成板级 ABI 或 monitor 接口”，继续读：

- `Examples/project/h747-lab/docs/h747_lab_spine_migration_boundary.md`
