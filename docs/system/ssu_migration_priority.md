# SSU 迁移优先级清单

## 目的

这份清单用于回答一个非常现实的问题：

不是所有 task 都值得立刻迁入 SSU 主线。
必须先把最能体现执行语义、最容易形成统一语言、最能压制旁路的那一批对象优先纳入。

因此，SSU 迁移不是“全仓平推”，而是分层推进。

## 迁移原则

### 原则 1：优先迁移“执行语义最强”的 task

优先迁入那些本质上承担“推进、drain、pump、loop、defer、budgeted step”职责的 task。
这些对象最能体现 SSU 的价值。

### 原则 2：优先迁移“旁路风险最大”的 task

凡是容易长出：

- 自己的推进循环
- 自己的 timeout 逻辑
- 自己的隐式唤醒路径
- 自己的 task/ISR 混合语义

都应优先进入 SSU 收敛视野。

### 原则 3：暂缓迁移“数据面主导”的执行路径

像 audio data plane 这种设备时钟主导、IRQ 强耦合、pull first 的路径，
这一阶段不适合强行纳入统一行为模型。
应先保留在文档映射层，而不是立刻做代码主线重构。

## 第一层：P0（必须优先纳入）

这类对象是 SSU 主线最优先覆盖的目标。

### 1. Pump Task

典型特征：

- 周期推进
- budgeted
- 负责驱动一个 service/reactor/router
- 天然适合 `task_only + timer/io_ready + non_blocking`

当前已纳入示例：

- `system.reactor_pump`
- `input.pump`
- `canopen.pump`

后续所有新 pump task，都应默认补齐 `ssu_meta()`。

### 2. Reactor 驱动 Task

典型特征：

- ISR-safe notify
- task-context drain
- budgeted + resubmit
- 最容易形成“执行语义统一”的标准样板

优先级理由：

- 它天然对应 `io_ready-submit`
- 它天然暴露上下文切换问题
- 它天然最容易长出旁路

### 3. Service Loop / Protocol Progress Task

典型特征：

- 负责推进协议栈状态
- 以 poll / progress / retry / deadline 的形式存在
- 容易自带超时、自带小循环、自带内部状态推进通道

优先级理由：

- 这类 task 最能决定 Charm 最终会不会重新长出“协议栈自有执行模型”

## 第二层：P1（随后纳入）

这类对象应在第一批 pump/service task 收敛后纳入。

### 4. RunLoop / Frame / Phase Step

典型特征：

- 每帧推进
- update/render/idle phase 明确
- 与 UI 或系统主循环强相关

优先级理由：

- 这类对象能验证 `frame` trigger 是否需要成为 SSU 的真实一等语义
- 但在第一阶段，它们还不应打乱主线收敛顺序

### 5. UI 驱动类 Task

典型特征：

- 与 input/update/render 等协同
- 往往挂在 run loop 周围
- 很容易和“事件驱动”产生双轨制

优先级理由：

- UI 是验证 SSU 是否能穿透“帧推进世界”的关键
- 但如果过早进入，会把阶段目标从执行语义收敛拖向整套 UI 架构重构

### 6. 协议栈推进类 Task（复杂版）

典型特征：

- 不只是 pump 一步推进
- 可能包含 request/response、重试、session、窗口推进等复合语义

优先级理由：

- 它们非常关键
- 但应该建立在 P0 pump/service 已经形成稳定语言之后再纳入

## 第三层：P2（暂缓）

这类对象当前应主要停留在“文档映射”与“边界声明”层面。

### 7. Audio Data Plane

典型特征：

- device clock 主导
- DMA/IRQ 节拍驱动
- pull first
- jitter 敏感
- 数据面和控制面分离明显

当前策略：

- 映射到 SSU 的 `demand` 语义
- 但不急于迁成统一行为主线
- 先保持文档层一致，不强推代码层统一

### 8. 强 IRQ 耦合路径

典型特征：

- 依赖极短 ISR 路径
- 后续工作通过 defer/offload 转移
- 数据或设备节拍强主导

当前策略：

- 保持“ISR only / defer to task”边界清晰
- 不在这一阶段把它们强制折叠成统一编码风格

## 迁移动作模板

每个候选 task 纳入 SSU 时，统一按以下动作推进：

1. 补 `ssu_meta()`
2. 进入 `TaskRegistry` 后可被 `task_ssu_name()` 识别
3. 在 `tasks.json` / `trace` 中可见标签
4. 若属于严格模式 target，必须通过编译期约束
5. 若存在旁路执行路径，需要记录是否后续回收

## 当前已纳入对象

- `system.reactor_pump`
- `input.pump`
- `canopen.pump`

## 当前应优先排查的下一批对象

建议优先继续寻找：

- 新的 pump task
- 新的 reactor 驱动 task
- service loop / protocol progress task
- run loop 中明确承担 phase 推进职责的对象

## 当前明确暂缓的对象

- audio data plane
- 设备时钟强主导的数据路径
- 需要大规模牵动 runtime/IRQ/driver 行为的对象

## 一句话执行规则

先统一最像“执行单元”的对象，
再去碰那些最像“系统世界”的对象。
