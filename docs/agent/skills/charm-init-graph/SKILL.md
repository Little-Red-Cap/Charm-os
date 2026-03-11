# charm-init-graph

用途：
- 用于新增能力时的装配路径设计与落地检查。
- 约束 board_caps / driver / init.node / bringup 的统一链路。

适用场景：
- 新增板级能力或驱动能力
- 新能力接入 init.graph
- Core/Board/extra 链路梳理

不适用场景：
- 纯业务功能实现
- 与装配无关的模块优化

依赖规则：
- `../../rules/charm-architecture.md`
- `../../rules/embedded-modern-cpp.md`
- 必要时参考 `../../rules/collaboration.md`

---

## 工作流程

1. 明确 Capability 名称与边界（对外能力名，不泄漏实现细节）
2. 定义 provides/requires（cap_id 与依赖方向必须清晰）
3. 实现 init.node 并接入 Core/Board/extra 链
4. 校验无隐式 init，所有能力从 init.graph 装配
5. 在 bringup 链中做最小验证（能 open/能用）

---

## 产出要求

- Capability 命名与 provides/requires 清单
- init.node 接入位置（Core/Board/extra）
- 最小验证路径（open/初始化/基础行为）

---

## 检查要点

- 是否绕过 init.graph
- 是否引入隐式初始化或顺序依赖
- 是否破坏能力边界
