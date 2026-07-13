# GPIO Input/Output/Edge Readiness

> **文档状态：`exploration`（尚无统一 mock）**

现行接口状态见 [`../architecture/gpio_device_contract_v0.md`](../architecture/gpio_device_contract_v0.md)。

## 当前事实

- 仓库有 `hal_gpio` 的 init/read/write 形状；
- 没有统一的 GPIO level/edge script backend；
- pin ownership、edge queue、IRQ/task 转发和去抖没有公共契约。

## 最小推进条件

先分别验证一个 output 和一个 input consumer，再增加 edge：

- output 写入与 active level；
- input 采样及 backend failure；
- edge 顺序、丢失/溢出和 detach；
- ISR 只记录事件，task context 处理回调。

LED、button 或 Host fixture 名称不能替代真实板来源说明。
