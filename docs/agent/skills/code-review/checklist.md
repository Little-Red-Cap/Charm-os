# Code Review Checklist

本清单用于 Charm 项目代码审查的检查辅助。
请结合以下规则使用：

- `../../rules/charm-architecture.md`
- `../../rules/embedded-modern-cpp.md`
- 涉及事件连接时额外结合 `../../../architecture/signal_state_contract_v0.md`

本清单不是规则来源，而是 review 时的检查辅助。

---

## 1. 阻断项检查

### 1.1 运行时与内存
- [ ] 是否引入动态分配
- [ ] Kernel/驱动/实时回调中是否使用动态容器
- [ ] 是否引入异常或 RTTI

### 1.2 初始化与装配
- [ ] 是否绕过 `init.graph`
- [ ] 是否把底座能力放入未定义的旁路节点
- [ ] 是否引入散落的临时 Caps

### 1.3 IO 与协议纪律
- [ ] `io::Channel::read/write` 是否非阻塞
- [ ] 是否出现 `Ok(0)`
- [ ] 暂不可用时是否返回 `Errc::would_block`
- [ ] 协议层是否出现 busy-spin/阻塞/睡眠/自定义超时循环
- [ ] 是否绕过 `io.reactor` / Kernel / EDA
- [ ] 是否绕过 `io.registry` 或显式 context 注入
- [ ] 业务层是否直接轮询硬件

### 1.4 错误模型
- [ ] 是否使用 `util::Errc`
- [ ] 是否使用 `util::Result<T>`
- [ ] 是否吞掉错误码
- [ ] 是否自建不统一的 Error 结构

### 1.5 时间源
- [ ] 是否绕过 `charm.system.clock`
- [ ] 是否自建 `now_ms/now_us`
- [ ] 是否未通过统一 Clock 注入获取时间

### 1.6 模块分层
- [ ] 是否违反 Foundation→Runtime→IO→Domain 依赖方向
- [ ] 是否存在反向渗透
- [ ] 是否暴露不该暴露的内部细节

### 1.7 事件连接纪律
- [ ] 是否把 `signal.emit()` 用成了队列、mailbox 或隐式异步
- [ ] 跨 ISR / task / reactor / scheduler submit 边界时是否仍在 direct `emit()`
- [ ] `deferred_signal` / `poster.post()` 是否偷偷退化成 direct call
- [ ] 是否在 ISR 中直接 `emit()` 非 irq-safe 槽或直接 `state.set()`
- [ ] 是否把 `state` 当成命令总线、event log 或 replay 队列
- [ ] 是否在 `emit` 过程中修改同一 `signal` 的连接拓扑
- [ ] 是否把长期 wiring 藏在构造过程的匿名 `connect` 中，而没有进入 `init.connection` / `materialize`
- [ ] target 生命周期是否明显覆盖 connection 生命周期

---

## 2. 重要问题检查

### 2.1 设计建模
- [ ] 是否用运行期分支替代编译期建模
- [ ] 是否用注释替代类型语义
- [ ] 是否使用 `void*` 或裸指针接口
- [ ] 是否依赖隐式顺序/约定传递语义

### 2.2 抽象与接口
- [ ] 接口是否足够强类型
- [ ] 是否优先使用 `std::span` / `std::array` 等视图类型
- [ ] 抽象成本是否可解释
- [ ] 是否因“熟悉”而回退传统写法

### 2.3 模块组织
- [ ] 是否不必要地回退 `.h/.cpp`
- [ ] 是否用物理拆分替代语义分层
- [ ] 是否存在职责边界不清

### 2.4 输出与日志
- [ ] 是否使用 `printf/snprintf` 替代 `out::format/out::printf`
- [ ] 是否在实时路径做格式化/分配
- [ ] 日志路径是否混乱

### 2.5 第三方与构建
- [ ] 是否引入不可控第三方依赖
- [ ] 核心路径是否引入联网依赖
- [ ] 第三方库是否缺少开关
- [ ] 是否遗漏 PC/MCU 双端构建要求

---

## 3. 逃生舱检查

- [ ] 妥协是否有明确理由
- [ ] 范围是否最小化
- [ ] 是否边界隔离
- [ ] 是否有标记与文档
- [ ] 是否有复审或移除计划

---

## 4. 优化建议检查

- [ ] 命名是否清晰稳定
- [ ] 接口是否可进一步收敛
- [ ] 是否可加强类型约束
- [ ] 运行时成本是否过高且不可解释
- [ ] 适配层与核心逻辑隔离是否足够

---

## 5. 输出建议

建议按以下顺序组织：

1. 总体结论
2. 阻断问题
3. 重要问题
4. 优化建议
5. 未确认但值得关注的风险点
