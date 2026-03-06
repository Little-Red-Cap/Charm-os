# Power/Low‑Power 架构总览

目标：把功耗管理从“平台细节”提升为“统一策略 + 能力约束”，保证 PC/MCU 行为可对齐。

## 1. 分层与责任

- **Foundation**：仅提供单位/容器，不含功耗逻辑。
- **Runtime/System**：功耗策略、状态机、唤醒/时钟约束合并。
- **Platform**：平台实现（时钟切换、睡眠指令、唤醒源配置）。

## 2. 状态模型

```
active -> idle -> sleep -> deep_sleep -> stop -> standby
```

含义（建议）：
- `active`：全功能运行
- `idle`：CPU idle，外设运行
- `sleep`：低速时钟，保留 SRAM
- `deep_sleep`：深度休眠，部分时钟关闭
- `stop`：几乎全停，靠唤醒源恢复
- `standby`：最低功耗，可能复位恢复

## 3. 唤醒源与时钟域

### 唤醒源（WakeSource）
- irq / wake_pin / rtc / dma / timer / usb / other

### 时钟域（ClockDomain）
- core / ahb / apb1 / apb2 / peripheral

## 4. 核心接口（草案）

### Policy

```
Policy::constraints()
Policy::choose_target(snapshot)
```

### Manager

```
Manager::request(state)
Manager::add_wake_source(req)
Manager::add_clock_domain(req)
Manager::decide_target()
Manager::enter_state(state)
Manager::exit_state(state)
```

## 5. 运行流程（简化）

1. 上层请求目标状态（request）
2. 汇总唤醒源与时钟域约束
3. Policy 选择目标状态（decide_target）
4. 进入低功耗（enter_state）
5. 唤醒后恢复（exit_state）

## 6. 约束与规则

- **决策在 Runtime**，执行在 Platform
- **唤醒源/时钟域变化必须可追踪**（trace）
- **策略不允许直接操纵硬件**，只输出“目标状态”

## 7. 后续落地计划

- Windows: 空实现（仅记录与模拟）
- STM32: Stop/Standby + RTC/EXTI 唤醒
- USB: 作为唤醒源之一接入

