# ARMv7-A 平台契约

这份文档用于把 ARMv7-A 裸机 bring-up 的边界写清楚，避免仓库再次滑回“公共层知道太多板级私货”的状态。

## 一句话版本

- 公共层负责“启动什么、何时切换、切换前要满足什么条件”
- 平台叶子负责“怎么把具体机器切到能执行下一阶段的状态”

只要这句话不变，我们就能同时支撑：

- QEMU 上的 Cortex-A bring-up 验证
- RK3506 这类真实 Cortex-A7 板级适配
- 后续别的 ARMv7-A 目标

## 平台叶子应该负责什么

平台叶子应该收住这些动作：

- 早期 reset 收口
- 异常向量安装和 VBAR 所有权
- CPU 级 IRQ/FIQ 屏蔽与路由
- 中断控制器静默、摘线与重新接线
- 早期串口
- GIC 和 generic timer 接线
- MMU 属性切换
- cache / TLB / barrier 顺序
- 最终 branch 或进入主逻辑前的机器状态准备

这些动作天然和 SoC、板级布局、BootROM 前置状态有关，所以它们必须停留在 leaf target 或 leaf module。

## 公共层不应该负责什么

下面这些不应被平台 bring-up 反向塑形成公共接口：

- 镜像格式和校验策略
- slot / rollback / pending-active 语义
- UART 下载协议
- Flash 分区和存储拓扑
- 某块板子的 UART/GIC/MMIO 常量
- 某个 example 的启动流程编排

换句话说，公共层可以知道“需要一次平台切换”，但不应该知道“RK3506 的 UART0 在哪里、GICD 在哪里、要不要碰哪一个 GRF 位”。

## QEMU 与 RK3506 的关系

当前更健康的推进方式是：

- `Examples/kernel/armv7a/qemu` 继续承担 Cortex-A 通用 bring-up 验证职责
- `targets/rk3506` 作为真实板级叶子，逐步映射这套平台语义

QEMU 先验证的是顺序和契约：

- reset 早期动作
- 向量表安装
- 中断控制器接线
- 异常与 abort 路径
- cache / TLB / MMU 操作顺序

RK3506 之后负责把这些语义落在真实地址和真实外设上。

## RK3506 当前建议边界

对 RK3506 而言，当前最小叶子职责应该先收在下面这组能力里：

- UART0 最小输出
- VBAR 安装和低向量切换
- GIC distributor / CPU interface 基地址
- generic timer 频率和 IRQ 路由
- 链接地址、栈顶和最小可运行内存布局

而下面这些先不要和当前主线绑死：

- 多阶段 stage1/stage2 boot chain
- BootROM 下载协议细节
- RockUSB / loader 模式
- 多核 release / PSCI 完整接线
- 复杂 DDR 训练或安全启动语义

## 推荐推进顺序

1. 先把单镜像 bare-metal skeleton 跑通
2. 接真实的 UART/CRU/GRF，去掉“串口已可用”的假设
3. 接 GIC + generic timer 烟测
4. 再把 QEMU 已验证过的异常/向量/平台 hook 逐步映射到 RK3506
5. 最后再推进 MMU、cache/TLB、SMP、BootROM 媒介链路

这个顺序的核心不是保守，而是避免把多个耦合面一次性绑在一起。
