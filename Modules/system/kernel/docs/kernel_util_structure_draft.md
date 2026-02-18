# Charm-os 内核+基础设施（kernel + utilities）结构草�?
> 目标：在 Modules 中以 C++26 Modules 构建“可验证、零运行期成本、可裁剪”的内核与基础设施；通过能力注入与编译期配置实现平台无关，先�?Windows 上验证�?
---

## 1. 迁移范围与目标边�?
- 范围：kernel + utilities
- 不做：HAL / component / shell（后续阶段）
- 运行环境：无异常、无 RTTI、零动态分�?- 语言：C++26 Modules
- 默认模型：编译期配置 + 能力注入

---

## 2. 目录结构（Modules�?
```
Modules/
├── util/
�?  ├── core.cppm                # 基础类型、常量、断言、编译期工具
�?  ├── type_list.cppm           # 类型列表与元编程
�?  ├── expected.cppm            # std::expected 适配与简化工�?�?  ├── span.cppm                # std::span 封装（适配 alias�?�?  ├── static_array.cppm        # 固定容量容器
�?  ├── bitset.cppm              # 编译�?bitset
�?  ├── units.cppm               # 时间单位类型（tick / ms / us�?�?  └── contract.cppm            # 轻量契约（static/consteval 校验�?�?├── kernel/
�?  ├── config.cppm              # Kernel Config（consteval 校验�?�?  ├── capabilities.cppm        # 能力注入接口（时间源/原子�?唤醒/调度钩子�?�?  ├── evt.cppm                 # 事件类型与事件消�?�?  ├── evt_queue.cppm           # 事件队列（ring/list 策略�?�?  ├── task_state.cppm          # 类型级状态机（Created/Ready/Running/Stopped�?�?  ├── eda.cppm                 # EDA 任务（核心）
�?  ├── scheduler.cppm           # 调度器（多优先级�?�?  ├── timer.cppm               # TEDA/定时器队列（可选）
�?  └── sync.cppm                # 同步原语（可选）
�?└── platform/
    └── win/
        ├── time_source.cppm     # Windows 时钟能力注入实现
        ├── irq_guard.cppm       # Windows 原子区模�?        └── wakeup.cppm          # 事件唤醒模拟
```

> 说明�?> - Modules 仅存放可复用、平台无关的核心；平台实现放 `Modules/platform/*`，但仍以能力注入方式连接�?> - �?C 风格 API 不直接迁入；保持“新内核 API 自洽”�?
---

## 3. 编译期配置（Config 模型�?
**原则**：无宏语义，所有核心配置走类型 + consteval 校验�?
示例方向（伪代码，仅示意）：

```cpp
export module kernel.config;

export struct KernelConfig {
    static constexpr bool enable_timer = true;
    static constexpr bool enable_dynamic_priority = false;
    static constexpr std::size_t priority_levels = 4;
    static constexpr std::size_t evtq_capacity = 64;
};

consteval void validate(KernelConfig cfg) {
    static_assert(cfg.priority_levels >= 1);
    static_assert(cfg.evtq_capacity >= 8);
}
```

**裁剪策略**：未启用的模块不参与编译期实例化（模块分�?+ `if constexpr`）�?
---

## 4. 能力注入（Capability Injection�?
**目标**：平台相关能力从核心剥离，内核仅依赖概念�?
计划注入能力�?- `TimeSource`：当�?tick、tick 差值、定时器驱动
- `IrqGuard`：进�?退出临界区（原子区�?- `Wakeup`：从 idle 唤醒调度�?- `SwiTrigger`：多优先级队列激活策略（PC 上模拟）

核心约束通过 `concept` 表达；平台实现放�?`Modules/platform/*`�?
---

## 5. 类型级状态机（Type-state�?
**目标**：将任务生命周期与调用序列固化到类型系统�?
示例方向�?
```
Task<Created> -> init() -> Task<Ready>
Task<Ready>   -> start() -> Task<Running>
Task<Running> -> stop()  -> Task<Stopped>
```

- 任务 API 只能在合法状态下调用（编译期约束）�?- 对应 EDA/TEDA 的启�?挂起/终止接口强类型化�?
---

## 6. 事件队列策略（第一阶段�?
采用 **多优先级 + ring buffer** 的最窄路径：
- 静态容量（`std::array` / 自定�?fixed ring�?- 每优先级一个队�?- 可选动态优先级策略�?TODO

后续扩展�?- list-based 队列（支持动态优先级�?- 事件消息（evt + msg�?
---

## 7. Windows 端验证入�?
- 入口文件：`Draft/Examples/windows/main.cpp`
- 目标：构建最�?demo：创�?1~2 �?EDA 任务，投递事件、跑调度�?- 不引�?SDL 依赖（除非必要）

---

## 8. 约束与约�?
- 零动态分�?- 无异�?RTTI
- 只用 C++26 模块
- UTF-8�? 空格缩进
- 注释英文（不强制�?
---

## 9. 第一阶段里程�?
1. `util.core` + `kernel.config` + `kernel.evt` 完成
2. `kernel.evt_queue`（ring buffer）完�?3. `kernel.eda` + `kernel.scheduler`（多优先级）完成
4. Windows main 验证：事件投递与调度循环

---

## 10. 待确认问题（后续跟进�?
- 是否需�?TEDA/定时器队列同步推进？
- 是否需要同步原语（mutex/sem/queue）第一阶段落地�?- 是否有既定对�?API 需要保持风格一致？

---

以上为结构草案，后续将按你确认的特性范围细化接口设计�?
