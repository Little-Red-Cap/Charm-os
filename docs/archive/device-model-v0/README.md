# Device Model v0 讨论归档

> `status`: `archived`

早期 `device_model_overview.md` 试图为 USB、FS、Audio、IO、Kernel/ModuleX 建立统一 `Device/Driver/Bus/Registry` 生命周期。当前源码只证明了运行期 discovery 子系统和少量 USB/stable-slot 样本，不能支持该范围的统一化主张。

现行入口改为：

- [`../../architecture/driver_model.md`](../../architecture/driver_model.md)

## 保留的讨论价值

- 动态设备需要显式描述、匹配、probe/init/remove 和 suspend/resume。
- 匹配冲突应有确定规则，不能依赖遍历偶然性。
- 运行期设备必须考虑 detach 后已经发放的入口如何失效。
- static controller 与 runtime-discovered device 不应强行套用同一生命周期。
- `try_* + util::Result` 比新增第三套错误约定更适合继续收敛。

## 未实施或不再作为默认方向

- FS、Audio、socket endpoint 和 ModuleX 尚未统一迁入 `device::Registry`。
- USB Device controller 不应整体视为动态 bus/device。
- Power/PM、多总线协同和完整 revoke 仍没有统一实现。
- VSF 对照仅是早期接口形状参考，不构成 Charm 契约来源。
- 旧文档中的 USB 注册代码是说明性草图，不是 canonical example。

这些方向若重新推进，应先给出具体消费者、失败语义和可运行证据，再更新现行契约。
