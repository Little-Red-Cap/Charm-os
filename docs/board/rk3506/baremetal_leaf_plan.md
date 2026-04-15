# RK3506 Bare-metal Leaf Plan

这份文档描述 RK3506 当前推荐的推进模型：先把它做成一个单镜像 bare-metal 板级叶子，再逐步对接真实板子，而不是继续把公开入口建成 staged boot chain。

## 当前目标

短期目标不是完整 Bootloader，而是：

- 能稳定生成 RK3506 裸机镜像
- 有最小早期串口输出
- 有明确的向量表安装和 VBAR 所有权
- 有后续接 GIC、generic timer、MMU、cache/TLB 的清晰落点

## 当前已经收住的边界

- `targets/rk3506` 现在作为显式 leaf target 接入
- 公开构建入口收敛为单镜像 `rk3506-baremetal-skeleton`
- `startup.S` 只负责清 BSS、调用平台 reset hook、安装向量、进入主入口
- `rk3506_platform.hpp/.cpp` 负责早期平台动作和常量布局

## 旧 stage1/stage2 草案怎么处理

旧草案里有些想法仍然有价值：

- 跳转前的环境快照
- 向量表切换意识
- cache / TLB / barrier 维护意识

但这些内容后续应以下面两种形式保留，而不是继续作为当前公开模型：

- 收进 RK3506 平台叶子的实现细节
- 在 QEMU 通用 ARMv7-A 路线上先验证顺序，再回填到 RK3506

## 下一批最值得做的事

1. 去掉“UART0 已经被前级初始化”的假设
2. 把 CRU / GRF / pinmux / reset 相关最小寄存器接进来
3. 做一个 RK3506 单核 GIC + generic timer 的最小烟测
4. 把异常向量和 abort/IRQ 处理接口对齐到 QEMU 那条公共契约
5. 再推进 MMU 属性切换、cache/TLB 维护和真实上板验证

## 暂时不要过早绑定的部分

- RockUSB / loader 模式细节
- 多核 release 和 PSCI 完整路径
- 复杂下载链路
- 安全启动、OTP、量产烧录语义

这些都重要，但不应该先压进当前最小 bring-up 模型里。
