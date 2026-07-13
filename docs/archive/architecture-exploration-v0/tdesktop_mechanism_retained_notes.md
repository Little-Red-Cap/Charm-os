# Telegram Desktop 机制比较保留笔记

> `status`: `archived`

本文只保留外部大型产品引出的审查问题，不证明 Telegram 或 Charm 的当前实现。引用上游机制前必须
重新检查源码与许可；外部项目采用某机制不构成 Charm 的采用理由。

## Lifetime 与 Execution Domain

- Reactive pipeline 必须明确 subscription、callback、target 和 pending delivery 的取消责任。
- MCU 实现还需声明固定容量、并发 emit、销毁时机和 queue/drop 行为。
- thread、ISR、task、reactor、UI 与 audio callback 的切换必须显式；一个 `post()` façade 不能隐藏
  blocking、allocation、IRQ safety、timer、shutdown 和 target lifetime 差异。
- Charm delegate/signal/state 是局部同步、non-owning 原语，不自动等价于 RPL 或通用 producer graph。

## Style、Schema 与 Platform

- 视觉 token 可以集中尺寸、字体和颜色，但不能演变为同时控制 layout、render、animation 与产品设计的
  全局 runtime；单位、fallback、memory cost 和编译产物仍由 UI contract 负责。
- 协议、descriptor 或持久化布局需要单一事实源时，可采用 `spec -> validate -> plan -> runtime`；只有
  存在跨语言/版本消费者、wire compatibility 或重复漂移时，schema/codegen 才优于局部 typed API。
- Platform adapter 用于限制 startup、MMIO、SDK、window system、threading 和 filesystem 差异传播，
  不消除差异，也不要求全局 Platform 基类。

## Persistent Storage

选择顺序二进制、TLV、KV、日志或数据库时必须明确：

- magic/version/length/integrity 与 unknown/missing field 处理；
- interrupted write、rollback、migration failure 和恢复策略；
- endian、alignment、wear、容量上限与 reader/writer compatibility fixture。

“字段只能 append”只适用于特定顺序格式，不是通用 storage 规则。

## 记录边界

注释应解释寄存器时序、cache/DMA coherency、barrier、ABI、errata、不变量、失败恢复和临时妥协的退出
条件，不逐行复述控制流，也不把未来方案写成事实。不得仅因外部项目使用 reactive library、submodule、
schema/codegen 或 platform adapter，就推导 Charm 需要同类公共机制。
