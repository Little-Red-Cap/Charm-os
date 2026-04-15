# RK3506 Target

这个目录现在表达的是一个单镜像的 RK3506 bare-metal 板级叶子。

它的职责不是把整个 Bootloader 链一次性铺满，而是先把以后真上板一定要存在的几个边界立住：

- RK3506 作为一个显式 leaf target 接入顶层构建
- Cortex-A7 早期入口、向量表安装、VBAR/屏障这类板级动作留在叶子里
- 公开地址布局、串口和中断控制器基地址留在板级目录
- 先产出一个可以稳定构建的 bare-metal skeleton，再继续接 GIC、timer、MMU 和 cache/TLB

## 当前模型

- `CharmTargetConfig.cmake`
  - 定义 `rk3506` 叶子 target
  - 暴露 `Charm::target::rk3506`
  - 声明这是一条 `armv7-a + bare-metal + cortex-a7` 的目标能力线
- `sources.cmake`
  - 维护 RK3506 显式 source list
  - 把 linker script 生成、编译宏和板级源文件绑定在一起
- `startup.S`
  - 负责清 BSS、调用平台早期 reset hook、安装异常向量，然后进入主入口
- `vectors.S`
  - 提供最小 ARM 向量表
- `rk3506_platform.hpp/.cpp`
  - 封装早期串口、VBAR 安装、低向量切换、地址布局和 MMIO 布局
- `rk3506_bootstrap.cpp`
  - 当前的 bare-metal skeleton 主入口
  - 只做最小日志与板级状态快照，然后停在 WFE

## 当前公开构建入口

- `rk3506-baremetal-debug`
  - 用于按 RK3506 叶子能力配置主工程
- `rk3506-baremetal-image-debug`
  - 用于生成 `rk3506-baremetal-skeleton`

## 设计取向

这里不再把 `stage1 -> stage2` 多阶段 handoff 当成当前主模型。

原因很直接：

- 我们现在的真实目标是先把代码跑到 RK3506 裸机上
- 公共 Cortex-A bring-up 边界应该围绕平台 reset、向量、异常、GIC、timer、MMU 展开
- 过早把公开入口建成 staged boot chain，会把板级 bring-up 和 Bootloader 策略绑死在一起

之前那批 stage1/stage2 草案里有价值的部分，比如环境快照、向量切换、cache/TLB 维护意识，并没有被否定；只是它们后续应该回到平台叶子的实现细节里，而不是继续作为当前对外模型。

## 当前假设

- `UART0 @ 0xff0a0000`
- `reg-shift = 2`
- `GICD @ 0xff581000`
- `GICC @ 0xff582000`
- generic timer 频率暂按 `24 MHz`
- 当前最小串口输出仍假设 BootROM 或更早阶段已经把 UART0 时钟带起来

## 下一步

- 接入真实的 CRU/GRF/UART 早期初始化，而不是继续假设串口已经可用
- 做 RK3506 单核 GIC + generic timer 烟测
- 把 QEMU 上已经验证过的异常/向量/平台契约逐步映射到 RK3506
- 再推进 MMU 属性切换、cache/TLB 维护和真实板级内存布局
