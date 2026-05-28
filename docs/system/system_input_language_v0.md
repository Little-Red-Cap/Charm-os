# System Input Language v0

本文档定义 Charm System Compiler 的核心输入语言 v0。

当前阶段不追求完整 DSL 或大规模 codegen，而是先把 **SystemSpec / Profile / BoardPackage / Binding / Facet** 这五个输入词汇与仓库现有载体对齐，形成可验证的概念映射表。

## 1. 为什么需要输入语言

Charm 的 System Compiler 已经有了强大的 **输出语言**：

- `materialized_graph` - 装配后的能力图
- `artifact_report` - 系统编译结果的统一结论
- `bringup_evidence` - 启动证据
- `resource_contract` - 资源契约
- `explain_surface` - 可追问面

但输入侧仍然分散在：

- case manifest 的 `subject` 字段
- CMake 的 profile/featureset 配置
- BoardCaps 的板级事实
- init.graph 的 capability 依赖声明

**System Input Language v0 的目标**：把输入侧词汇统一收敛，让"系统如何被声明"与"系统如何被验证"使用同一套语言。

---

## 2. 核心输入词汇

### 2.1 SystemSpec

> **这个系统想成为什么样。**

| 属性 | 说明 |
|------|------|
| **关注点** | 系统级目标，而非单个模块 |
| **典型问题** | 要暴露哪些 capability？走哪条 bringup 路径？哪些 facet 应处于活动状态？ |
| **当前载体** | `export_case_manifest` 的 case entry、`artifact_report.system_input.system_spec` |
| **不应误解为** | 单个 CMakeLists.txt / 单个 init chain / 单个导出 case 名字 |

**当前映射**：

```json
// export_case_manifest 中的 SystemSpec 投影
{
  "case_name": "bringup-minimal-observe-demo",
  "case_kind": "materialized_graph",
  "source": "Examples/init/bringup_minimal_observe_demo",
  "build_dir": "cmake-build-init-bringup-minimal-observe-clang",
  "build_target": "init-bringup-minimal-observe-demo",
  "export_target": "export_bringup_minimal_materialized_graph"
}
```

### 2.2 Profile

> **这个系统允许活在哪种资源宇宙里。**

| 属性 | 说明 |
|------|------|
| **关注点** | 资源/功能等级，而非单个模块开关 |
| **典型问题** | 是否允许堆分配？支持哪些 UI 功能集？调度精度要求？ |
| **当前载体** | `charm_apply_target_profile(...)`、`CMakeFeatureset`、`artifact_report.system_input.resolved_input.profile` |
| **不应误解为** | Debug/Release / 单个 UI featureset / 编译器选项集合 |

**当前映射**：

```json
// artifact_report 中的 Profile 投影
{
  "profile": {
    "value": "MCU_MIN",
    "source": "explicit_argument"  // 或 "case_subject" / "default"
  }
}
```

**常见 Profile 变体**：

- `MCU_MIN` - 最小资源约束，无堆分配
- `MCU_FULL` - 完整功能集，允许堆分配
- `HOST_DEBUG` - PC 调试模式

### 2.3 BoardPackage

> **板级已知事实是什么。**

| 属性 | 说明 |
|------|------|
| **关注点** | 板级事实声明，而非生命周期推进 |
| **典型问题** | 有哪些外设？时钟配置？默认 capability 绑定？ |
| **当前载体** | `platform::board::BoardCaps`、各板级 `make_board_caps()` |
| **不应误解为** | 隐式初始化函数 / BSP 钩子 / 运行期 probe 容器 |

**当前映射**：

```cpp
// platform/boards/*/board_caps.cppm 中的结构
namespace platform::board {
    struct BoardCaps {
        UartDesc uart1{};
        ClockDesc clock{};
        const char* console_cap{"io.console0"};
        InputDesc input{};
        SpiDesc spi1{};
        // ...
    };
}
```

**BoardPackage 的事实层次**：

1. **硬件事实**：外设类型、引脚配置、时钟参数
2. **能力事实**：提供的 capability 名称（如 `io.uart1`、`block.sd0`）
3. **绑定事实**：capability 之间的默认连接关系

### 2.4 Binding

> **这些事实如何连接到 capability、服务与最终系统结果。**

| 属性 | 说明 |
|------|------|
| **关注点** | 从板级事实到系统能力的技术路径 |
| **典型问题** | HAL 如何绑定到 Channel？Driver 如何注册为 capability？ |
| **当前载体** | `hal::*Binding`、`driver::*::ChannelBinding`、`io::ChannelAliasBinding`、`init chain` |
| **不应误解为** | 单纯的 `device::Driver` 类 |

**Binding 的双平面**：

```
静态 Capability 平面：
  BoardCaps → ControllerBinding → ServiceAdapter → capability export

动态 Discovery 平面：
  RuntimeBus → RuntimeDriver → capability export
```

**当前 Binding 类型**：

- `hal::UartBinding` - UART 硬件到 HAL capability
- `driver::usart::ChannelBinding` - HAL 到 IO Channel
- `io::ChannelAliasBinding` - Channel 到稳定 capability 入口
- `init::make_*_chain()` - 初始化依赖链

### 2.5 Facet

> **同一套架构在当前实例里，到底启用了哪些面。**

| 属性 | 说明 |
|------|------|
| **关注点** | 功能面的启用/禁用，而非单个模块 |
| **典型问题** | 启用 UI 吗？启用文件系统吗？启用 USB 吗？ |
| **当前载体** | `charm_add_runtime_facet(...)`、`active_facets`、`artifact_report.system_input.resolved_input.active_facets` |
| **不应误解为** | Profile / target / component |

**当前 Facet 列表**：

| Facet | 含义 |
|-------|------|
| `runtime` | 启用 EDA 调度器 |
| `input` | 启用输入系统 |
| `ui` | 启用 UI 系统 |
| `fs` | 启用文件系统 |
| `usb` | 启用 USB 系统 |
| `audio` | 启用音频系统 |

---

## 3. 输入词汇与 artifact_report 的映射

### 3.1 system_input 的完整结构

```json
{
  "system_input": {
    "system_spec": { /* 见 2.1 */ },
    "declared_input": {
      "subject": {
        "profile": null,
        "board": "stm32_stub",
        "active_facets": ["runtime", "input"]
      },
      "declared_facts": ["board.stm32_stub"],
      "declared_contracts": [
        { "contract": "needs_monotonic_clock", "requires": ["system.clock"] }
      ]
    },
    "resolved_input": {
      "profile": { "value": "MCU_MIN", "source": "explicit_argument" },
      "board": { "value": "stm32_stub", "source": "case_subject" },
      "active_facets": { "values": ["runtime", "input"], "source": "case_subject" },
      "subject_facts": [
        "profile.MCU_MIN",
        "board.stm32_stub",
        "facet.runtime",
        "facet.input"
      ]
    }
  }
}
```

### 3.2 输入来源的三种模式

| source 值 | 含义 |
|-----------|------|
| `explicit_argument` | 用户通过 CLI 参数显式指定 |
| `case_subject` | 从 case manifest 的 subject 字段继承 |
| `default` | 使用系统默认值 |

---

## 4. 资源契约作为验证面

Resource Contract 是 System Input Language 的第一个验证面：

> **"系统声明是否有资源合法性语言？"**

### 4.1 资源契约词汇

| 契约 | 含义 | 典型 requires |
|------|------|---------------|
| `may_block` | 允许阻塞等待 | `execution.may_block` |
| `needs_heap` | 要求堆分配能力 | `system.heap` |
| `needs_reactor` | 要求 Reactor 语义 | `io.reactor` |
| `needs_monotonic_clock` | 要求单调时钟 | `system.clock` |
| `irq_safe` | 允许 IRQ 上下文调用 | 无 |

### 4.2 契约审计结果

```json
{
  "fact_resolution": {
    "declared_contracts": 2,
    "audited_count": 2,
    "satisfied_count": 1,
    "violated_count": 0,
    "unknown_count": 1,
    "contracts": [
      {
        "contract": "needs_monotonic_clock",
        "state": "satisfied",
        "requires": ["system.clock"],
        "present_facts": ["system.clock"],
        "missing_facts": [],
        "fact_sources": ["graph_provided_facts"]
      },
      {
        "contract": "irq_safe",
        "state": "unknown",
        "requires": [],
        "present_facts": [],
        "missing_facts": [],
        "fact_sources": []
      }
    ]
  }
}
```

### 4.3 验证结论

Resource Contract 作为资源合法性语言 **有效**，因为：

1. **契约词汇已收敛**：五个核心词汇（may_block / needs_heap / needs_reactor / needs_monotonic_clock / irq_safe）已在 schema 中定义
2. **fact_inventory 分层清晰**：declared_facts / subject_facts / required_facts / graph_provided_facts / audit_provided_facts
3. **状态机完整**：satisfied / violated / unknown 三态覆盖

---

## 5. Explain Surface 作为验证面

Explain Surface 是 System Input Language 的第二个验证面：

> **"系统声明是否能被人和工具追问？"**

### 5.1 最小追问集

| 查询 | 作用域 | 回答 |
|------|--------|------|
| `cap list` | 单 report / root | 系统提供哪些 capability？它们的状态？ |
| `why unavailable <cap>` | 单 report / root | 为什么某个 capability 不可用？ |
| `graph path <cap>` | 单 report | 从某 capability 到根的完整路径是什么？ |
| `recent transitions` | 单 report | 最近发生了哪些状态切换？ |
| `resource summary` | 单 report / root | 资源契约的审计结果是什么？ |
| `bringup evidence` | 单 report / root | bringup 证据的聚合结果是什么？ |

### 5.2 追问结果示例

```
Query: why unavailable io.uart1
Result:
  capability: io.uart1
  state: unresolved_binding
  availability_state: unavailable
  missing_requires: ["hal.uart1"]
  reason: "required capability 'hal.uart1' is not provided by any materialized node"
```

### 5.3 验证结论

Explain Surface 作为追问面 **有效**，因为：

1. **查询语言已统一**：通过 `inspect_system_compiler_artifact_report.ps1` 统一入口
2. **作用域边界清晰**：单 report 查询 vs root 聚合各有明确边界
3. **compare 模式完备**：每个查询都支持 compare 模式，返回 drift 信息

---

## 6. 当前仓库映射表

| 输入词汇 | 当前主要载体 | v0 状态 | 下一步 |
|----------|-------------|---------|--------|
| `SystemSpec` | case manifest、`artifact_report.system_input.system_spec` | 已映射 | 补充 case_kind 语义 |
| `Profile` | target profile、CMake featureset | 碎片化 | 统一为 artifact_report profile 字段 |
| `BoardPackage` | `BoardCaps` + 板级 target/config | 事实载体存在 | 补充 fact 语义 |
| `Binding` | `*Binding`、init chain | 双平面存在 | 补充 binding_result 映射 |
| `Facet` | facet target、`active_facets` | 开始显式投影 | 补充 facet_matrix |

### 6.1 真实样例映射

以 `bringup-minimal-observe-demo` case 为例，各输入词汇的完整映射：

```
SystemSpec:
  case_name: bringup-minimal-observe-demo
  case_kind: materialized_graph
  source: Examples/init/bringup_minimal_observe_demo
  build_target: init-bringup-minimal-observe-demo

Profile:
  value: "" (未指定)
  source: "missing"

BoardPackage:
  value: "win_stub"
  source: "case_subject"
  subject_fact: "board.win_stub"

Facet:
  values: ["runtime", "input"]
  source: "case_subject"
  subject_fact: ["facet.runtime", "facet.input"]

Binding Result:
  resolved: [hal.uart1, input.router, input.service, io.reactor, io.registry, io.uart1, kernel.eda, system.clock]
  unresolved: [platform.irq]
```

---

## 7. 输入语言与输出语言的闭环

System Input Language v0 的核心价值在于建立**输入→输出闭环**：

```
输入声明                          输出验证
─────────                        ─────────
SystemSpec ─────────────────────→ case_kind
Profile ────────────────────────→ resolved_profile + resource_contract
BoardPackage ──────────────────→ subject_facts + binding_result
Binding ────────────────────────→ capability graph + unresolved_binding
Facet ──────────────────────────→ resolved_active_facets
```

当这个闭环闭合时，Charm 就能回答：

- 这个系统是否按照声明的方式成立了？
- 哪些声明没有被满足？
- 不满足的原因是什么？

---

## 8. case_kind 语义详解

`case_kind` 是 SystemSpec 的关键属性，区分两类系统实例：

### 8.1 materialized_graph

> 系统具有静态可物化的能力图。

**特征**：
- 有完整的 capability graph
- 可以生成 DOT/JSON 导出
- 支持 `graph path` 等静态图查询
- 包含 `artifacts.sample_json` 和 `artifacts.dot`

**典型场景**：
- `bringup-minimal-observe-demo` - bringup 演示
- `usb-msc-block-demo` - USB 存储设备演示

**artifact_report 期望字段**：
```json
{
  "structure": {
    "capability_count": 12,
    "node_count": 9,
    "edge_count": 8
  },
  "artifacts": {
    "sample_json": "materialized_graph.sample.json",
    "dot": "materialized_graph.dot"
  }
}
```

### 8.2 runtime_only

> 系统没有静态图，只有运行时观察。

**特征**：
- 没有 capability graph 导出
- 只有 `runtime_observe` sidecar
- `artifacts.sample_json` 和 `artifacts.dot` 为空
- 支持 `recent transitions` 等动态查询

**典型场景**：
- `usb-host-runtime-multi-smoke` - USB 主机运行时测试
- 纯运行时行为验证，不涉及静态装配

**artifact_report 期望字段**：
```json
{
  "structure": {
    "capability_count": 0,
    "node_count": 0,
    "edge_count": 0
  },
  "artifacts": {
    "sample_json": null,
    "dot": null,
    "runtime_observe": "*.runtime_observe.json"
  },
  "runtime_observe": {
    "publish_state_summary": { "published": 3, "missing": 0 },
    "recent_transitions": [...]
  }
}
```

### 8.3 case_kind 与 System Input Language 的关系

```
materialized_graph:
  输入 → 静态装配 → 可观测的静态图
  
runtime_only:
  输入 → 运行时行为 → 可观测的动态事件
```

两种 case_kind 都是 System Input Language 的合法实例，只是：
- `materialized_graph` 更适合验证"系统是否按声明装配"
- `runtime_only` 更适合验证"系统运行时行为是否符合预期"

---

## 9. Profile 解析路径

### 9.1 Profile 的三层结构

```
Profile 声明 → Profile 解析 → resolved_input.profile
```

**declared_profile**：用户/case 显式声明的 Profile

```json
{
  "declared_profile": "MCU_MIN"
}
```

**resolved_profile**：经过解析后的最终 Profile

```json
{
  "profile": {
    "value": "MCU_MIN",
    "source": "explicit_argument"
  }
}
```

### 9.2 Profile 来源的三种模式

| source 值 | 说明 | 典型场景 |
|-----------|------|----------|
| `explicit_argument` | CLI 参数显式指定 | `-Profile MCU_MIN` |
| `case_subject` | 从 case manifest 继承 | manifest 中声明 |
| `default` | 系统默认值 | 未指定时使用 |

### 9.3 常见 Profile 变体

| Profile | 含义 | 资源约束 |
|---------|------|----------|
| `MCU_MIN` | 最小配置 | 无堆分配、无异常、无 RTTI |
| `MCU_FULL` | 完整 MCU 配置 | 允许堆分配、部分 RTTI |
| `HOST_DEBUG` | PC 调试配置 | POSIX 兼容、调试符号 |
| `HOST_RELEASE` | PC 发布配置 | POSIX 兼容、优化级别 |

### 9.4 Profile 与 Resource Contract 的关系

Profile 直接影响 Resource Contract 的成立性：

```json
{
  "declared_profile": "MCU_MIN",
  "declared_contracts": [
    { "contract": "needs_heap", "requires": [] }
  ],
  "fact_resolution": {
    "needs_heap": {
      "state": "violated",
      "reason": "MCU_MIN profile does not provide system.heap"
    }
  }
}
```

---

## 10. BoardPackage Fact 扩展

### 10.1 subject_facts 的当前状态

```json
{
  "subject_facts": [
    "board.win_stub",
    "facet.runtime",
    "facet.input"
  ]
}
```

### 10.2 BoardPackage 可扩展的 Fact 列表

| Fact 前缀 | 含义 | 示例 |
|-----------|------|------|
| `board.*` | 板级标识 | `board.win_stub`、`board.stm32h747` |
| `arch.*` | 架构 | `arch.armv7-a`、`arch.riscv` |
| `platform.*` | 平台类型 | `platform.baremetal`、`platform.posix` |
| `has.*` | 可用外设 | `has.usb`、`has.eth`、`has.display` |

### 10.3 扩展示例

```json
{
  "subject_facts": [
    "board.stm32h747_player",
    "arch.armv7-m",
    "platform.baremetal",
    "facet.runtime",
    "facet.ui",
    "facet.usb",
    "has.display.rgb565",
    "has.audio.i2s"
  ]
}
```

---

## 11. BindingResult 映射

### 11.1 BindingResult 的当前状态

```json
{
  "binding_result": {
    "required_binding_count": 9,
    "resolved_binding_count": 8,
    "unresolved_binding_count": 1,
    "resolved_capabilities": [
      "hal.uart1", "io.reactor", "system.clock"
    ],
    "unresolved_capabilities": [
      "platform.irq"
    ]
  }
}
```

### 11.2 Binding 成立性判断逻辑

```
1. capability 是否被声明为 required？
   ↓ 是
2. 是否有节点提供该 capability？
   ↓ 是 → resolved
   ↓ 否 → unresolved (binding 未成立)
```

### 11.3 Blocker 的两类

| Blocker 类型 | 含义 | 典型原因 |
|--------------|------|----------|
| `node` | 节点阻塞 | 节点的 requires 未满足 |
| `binding` | 绑定阻塞 | required capability 无人提供 |

---

## 13. Resource Contract 覆盖度验证

### 13.1 现有契约词汇

| 契约 | 定义 | 状态 |
|------|------|------|
| `may_block` | 允许阻塞等待 | ✅ 已定义 |
| `needs_heap` | 要求堆分配能力 | ✅ 已定义 |
| `needs_reactor` | 要求 Reactor 语义 | ✅ 已定义 |
| `needs_monotonic_clock` | 要求单调时钟 | ✅ 已定义 |
| `irq_safe` | 允许 IRQ 上下文调用 | ✅ 已定义 |

### 13.2 现有 Case 的契约声明情况

| Case | declared_contracts | 声明的契约 |
|------|-------------------|-----------|
| `materialize-observe-demo` | `[]` | 无 |
| `connection-observe-demo` | `[]` | 无 |
| `bringup-block-observe-demo` | `[{contract: needs_monotonic_clock, requires: [system.clock]}]` | 1 个 |
| `bringup-minimal-observe-demo` | `[{contract: needs_monotonic_clock, requires: [system.clock]}]` | 1 个 |
| `usb-msc-block-demo` | `[]` | 无 |
| `usb-host-runtime-multi-smoke` | `[]` | 无 |

### 13.3 覆盖度评估

**已覆盖**：
- `needs_monotonic_clock` 已在 bringup 相关 case 中声明

**未覆盖**：
- 大部分 case 尚未声明资源契约
- `may_block`、`needs_heap`、`needs_reactor`、`irq_safe` 未在任何 case 中使用

### 13.4 下一步行动

1. **补充契约声明**：鼓励在 case manifest 中声明资源约束
2. **自动推断**：从 SSU 元数据自动推断 `may_block` 等契约
3. **Profile 联动**：让 Profile 隐式声明相关契约

---

## 14. Explain Surface 覆盖度验证

### 14.1 现有查询的覆盖情况

| 查询 | 单 report | root 聚合 | compare | 覆盖状态 |
|------|----------|-----------|---------|----------|
| `cap list` | ✅ | ✅ | ✅ | 完整 |
| `why unavailable` | ✅ | ✅ | ✅ | 完整 |
| `graph path` | ✅ | ❌ | ✅ | 部分 |
| `recent transitions` | ✅ | ❌ | ✅ | 部分 |
| `resource summary` | ✅ | ✅ | ✅ | 完整 |
| `bringup evidence` | ✅ | ✅ | ✅ | 完整 |

### 14.2 查询能力矩阵

| Capability 状态 | cap list | why unavailable | graph path |
|------------------|----------|----------------|------------|
| materialized | ✅ | ✅ | ✅ |
| published | ✅ | ✅ | - |
| unresolved | ✅ | ✅ | ✅ |
| blocked | ✅ | ✅ | ✅ |

### 14.3 覆盖度评估

**完整覆盖**：
- capability 的基本状态查询
- unresolved binding 的原因追踪
- resource contract 的审计结果

**部分覆盖**：
- `graph path` 不支持 root 聚合
- `recent transitions` 不支持 root 聚合

**待扩展**：
- 跨 case 的 capability 路径追踪
- 实时 transition 的 root 级聚合

---

## 15. 下一步工作

1. **补充 SystemSpec 的 case_kind 语义**：区分 `materialized_graph` 和 `runtime_only` ✅
2. **统一 Profile 解析路径**：让所有 Profile 解析都经过 `resolved_input` ✅
3. **扩展 BoardPackage fact**：把更多板级事实纳入 `subject_facts` ✅
4. **完善 BindingResult 映射**：让 binding 成立性结论进入正式报告 ✅
5. **验证 Resource Contract 覆盖度**：检查是否所有资源约束都已声明 ✅
6. **验证 Explain Surface 覆盖度**：检查是否所有 capability 都可被追问 ✅

---

## 16. System Input Language v0 总结

### 16.1 核心贡献

**建立了输入→输出闭环**：

```
输入声明                          输出验证
─────────                        ─────────
SystemSpec ─────────────────────→ case_kind
Profile ────────────────────────→ resolved_profile + resource_contract
BoardPackage ──────────────────→ subject_facts + binding result
Binding ────────────────────────→ capability graph + unresolved_binding
Facet ──────────────────────────→ resolved_active_facets
```

### 16.2 词汇表

| 词汇 | 关注点 | 当前载体 | 状态 |
|------|--------|----------|------|
| `SystemSpec` | 系统想成为什么样 | case manifest | ✅ |
| `Profile` | 资源宇宙 | target profile | ✅ |
| `BoardPackage` | 板级事实 | `BoardCaps` | ✅ |
| `Binding` | 能力连接 | `*Binding` | ✅ |
| `Facet` | 功能面 | `active_facets` | ✅ |
| `case_kind` | 实例类型 | materialized_graph / runtime_only | ✅ |

### 16.3 验证面

| 验证面 | 功能 | 状态 |
|--------|------|------|
| Resource Contract | 资源合法性审计 | ✅ 有效 |
| Explain Surface | 人类/工具追问 | ✅ 有效 |

### 16.4 Schema 映射

| Schema | 协议类型 | 用途 |
|--------|----------|------|
| `system_input_summary/v0` | Summary | 输入侧聚合 |
| `system_compiler_summary/v0` | Summary | 总结果聚合 |
| `binding_result_summary/v0` | Summary | Binding 结果聚合 |
| `bringup_order_summary/v0` | Summary | Bringup 顺序聚合 |
| `system_formation_summary/v0` | Summary | Formation 结果聚合 |
| `fact_resolution_summary/v0` | Summary | 资源契约聚合 |
| `system_compiler_result_map/v0` | Summary | 结果映射关系 |

### 16.5 v0 的价值

1. **术语统一**：五个输入词汇有了明确语义和仓库映射
2. **闭环建立**：输入声明和输出验证使用同一套语言
3. **工具支持**：通过 `inspect_system_compiler_artifact_report.ps1` 统一消费
4. **比较能力**：compare 模式支持跨 case 的漂移分析

### 16.6 v1 方向

1. **契约自动推断**：从 SSU 元数据自动生成 `declared_contracts`
2. **Profile 语义扩展**：让 Profile 隐式声明资源约束
3. **跨 case 图路径**：支持 root 级 `graph path` 聚合
4. **实时 Transition**：扩展 `recent transitions` 的 root 级聚合

---

## 17. 契约自动推断设计草案

### 17.1 背景

当前 Resource Contract 的 `declared_contracts` 需要手动在 case manifest 中声明，容易遗漏。

SSU 的 Meta 包含丰富的调度语义信息，可以作为契约推断的来源。

### 17.2 推断规则

#### 17.2.1 从 SSU Meta.blocking 推断

```cpp
// SSU Meta 定义
enum class BlockingKind : unsigned char {
    non_blocking,
    may_block,
};

// 推断规则
if (ssu_meta.blocking == BlockingKind::may_block) {
    add_contract({
        contract: "may_block",
        requires: ["execution.may_block"]
    });
}
```

#### 17.2.2 从 SSU Meta.domain 推断

```cpp
// SSU Meta 定义
enum class ExecutionDomain : unsigned char {
    isr_only,
    task_only,
    anywhere,
};

// 推断规则
if (ssu_meta.domain == ExecutionDomain::isr_only) {
    add_contract({
        contract: "irq_safe",
        requires: []
    });
}
```

#### 17.2.3 从 Capability 注册推断

```cpp
// 推断规则
if (node.provides.contains("io.reactor")) {
    // reactor 本身是提供者，不需要额外契约
    // 但需要 reactor 的节点应声明 needs_reactor
}

if (node.requires.contains("io.reactor")) {
    add_contract({
        contract: "needs_reactor",
        requires: ["io.reactor"]
    });
}

if (node.requires.contains("system.heap")) {
    add_contract({
        contract: "needs_heap",
        requires: ["system.heap"]
    });
}

if (node.requires.contains("system.clock")) {
    add_contract({
        contract: "needs_monotonic_clock",
        requires: ["system.clock"]
    });
}
```

### 17.3 推断流程

```
1. 收集所有 SSU 任务
   ↓
2. 遍历每个 SSU 的 Meta
   ↓
3. 应用推断规则
   ↓
4. 去重合并契约
   ↓
5. 与显式 declared_contracts 合并
   ↓
6. 输出 merged_contracts
```

### 17.4 Schema 扩展

```json
{
  "contract_inference": {
    "ssu_derived": [
      {
        "contract": "may_block",
        "source": "ssu_meta",
        "from_task": "audio_graph",
        "requires": ["execution.may_block"]
      }
    ],
    "capability_derived": [
      {
        "contract": "needs_reactor",
        "source": "requires",
        "from_node": "io.uart1",
        "requires": ["io.reactor"]
      }
    ],
    "user_declared": [...],
    "merged": [...]
  }
}
```

### 17.5 实施步骤

1. **v0**：在导出脚本中实现推断逻辑
2. **v1**：在 Schema 中正式定义 `contract_inference` 结构
3. **v2**：在编译器层面集成契约推断

### 17.6 价值

契约自动推断的价值在于：

1. **减少遗漏**：从代码元数据自动生成契约声明
2. **保持一致**：契约推断与实际行为保持同步
3. **提升覆盖度**：让所有 capability 的资源需求都被声明

---

## 18. Profile 隐式声明设计草案

### 18.1 背景

当前 Profile 只是资源宇宙的描述，没有隐式声明相关契约。

### 18.2 Profile 与契约的映射

```json
{
  "MCU_MIN": {
    "constraints": {
      "no_heap_after_boot": true,
      "no_exceptions": true,
      "no_rtti": true
    },
    "implicit_contracts": []
  },
  "MCU_FULL": {
    "constraints": {
      "heap_allowed": true
    },
    "implicit_contracts": [
      { "contract": "needs_heap", "requires": ["system.heap"] }
    ]
  },
  "HOST_DEBUG": {
    "constraints": {
      "posix_compatible": true,
      "debug_symbols": true
    },
    "implicit_contracts": []
  }
}
```

### 18.3 推断流程

```
declared_profile = "MCU_FULL"
    ↓
profile_config = lookup_profile("MCU_FULL")
    ↓
implicit_contracts = profile_config.implicit_contracts
    ↓
merged_contracts = user_declared + implicit_contracts
    ↓
fact_resolution = audit(merged_contracts)
```

---

## 20. 跨 Case 图路径聚合设计草案

### 20.1 背景

当前 `graph path` 查询只支持单 report 模式，不支持跨 case 聚合。

### 20.2 使用场景

**场景 A**：某 capability 在多个 case 中的提供路径对比

```
Case A (usb_audio): io.uart1 → hal.uart1 → platform.irq
Case B (usb_display): io.uart1 → hal.uart1 → platform.irq (共享路径)

问题：为什么 platform.irq 在 Case B 中是必需但未提供的？
```

**场景 B**：跨 case 的 capability 依赖热点

```
capability: io.reactor
  Case A: provided by kernel.eda
  Case B: provided by kernel.eda
  Case C: required but not provided
  Case D: provided by custom.reactor

观察：io.reactor 在 Case C 中缺失，应添加依赖
```

### 20.3 Schema 扩展

```json
{
  "graph_path_root_aggregation": {
    "query": {
      "kind": "graph_path_root",
      "capability": "io.reactor",
      "case_count": 4
    },
    "per_case_results": [
      {
        "case": "usb_audio",
        "state": "resolved",
        "provider_paths": [["kernel.eda"]],
        "status": "provided"
      },
      {
        "case": "usb_display",
        "state": "resolved",
        "provider_paths": [["kernel.eda"]],
        "status": "shared_with": ["usb_audio"]
      },
      {
        "case": "custom_reactor",
        "state": "resolved",
        "provider_paths": [["custom.reactor"]],
        "status": "provided"
      },
      {
        "case": "bringup_minimal",
        "state": "unresolved",
        "missing_requires": ["io.reactor"],
        "status": "required_but_missing"
      }
    ],
    "summary": {
      "provided_count": 3,
      "missing_count": 1,
      "missing_cases": ["bringup_minimal"],
      "shared_providers": {
        "kernel.eda": ["usb_audio", "usb_display"]
      }
    }
  }
}
```

### 20.4 实施步骤

1. **v0**：在 `New-GraphPathResult` 函数中添加 root 聚合支持
2. **v1**：在 Schema 中定义 `graph_path_root_aggregation` 结构
3. **v2**：支持跨 case 的路径对比和高亮

### 20.5 设计约束

| 约束 | 说明 |
|------|------|
| 只聚合 materialized_graph case | runtime_only 没有静态图 |
| 忽略完全相同的路径 | 减少噪音 |
| 突出差异路径 | 帮助发现配置漂移 |

---

## 21. 与其他文档的关系

- `docs/architecture/system_compiler_vocabulary_v0.md` - 完整词汇表（含输入/输出）
- `docs/system/artifact_report_v0.md` - 输出语言定义
- `docs/system/resource_contract_v0.md` - 资源契约语言
- `docs/system/explain_surface_v0.md` - 追问面定义
- `docs/system/bringup_evidence_pipeline_v0.md` - 证据流水线

---

## 22. System Input Language v0 完整结构

### 22.1 章节索引

| 章节 | 内容 | 状态 |
|------|------|------|
| 1 | 为什么需要输入语言 | ✅ |
| 2 | 核心输入词汇 | ✅ |
| 3 | 输入词汇与 artifact_report 的映射 | ✅ |
| 4 | Resource Contract 验证面 | ✅ |
| 5 | Explain Surface 验证面 | ✅ |
| 6 | 当前仓库映射表 | ✅ |
| 7 | 输入→输出闭环 | ✅ |
| 8 | case_kind 语义 | ✅ |
| 9 | Profile 解析路径 | ✅ |
| 10 | BoardPackage fact 扩展 | ✅ |
| 11 | BindingResult 映射 | ✅ |
| 13 | Resource Contract 覆盖度验证 | ✅ |
| 14 | Explain Surface 覆盖度验证 | ✅ |
| 16 | v0 总结 + v1 方向 | ✅ |
| 17 | 契约自动推断设计草案 | ✅ (新增) |
| 18 | Profile 隐式声明设计草案 | ✅ (新增) |
| 20 | 跨 Case 图路径聚合设计草案 | ✅ (新增) |

### 22.2 核心贡献总结

1. **术语统一**：SystemSpec / Profile / BoardPackage / Binding / Facet 有了明确语义
2. **case_kind 二分法**：materialized_graph vs runtime_only
3. **输入→输出闭环**：输入声明与输出验证使用同一套语言
4. **v1 设计草案**：契约自动推断 / Profile 隐式声明 / 跨 case 图路径
