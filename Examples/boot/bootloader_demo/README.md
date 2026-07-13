# bootloader_demo

## 状态

- `scope`: host-only boot pipeline fixture
- `source`: [`main.cpp`](main.cpp)
- `contract`: [`docs/boot/README.md`](../../../docs/boot/README.md)

fixture 使用内存 mock storage 构造 Slot A/B 镜像，并验证：

- X/YMODEM 写入 Slot B、缺 header 失败与 payload verify；
- `BootPlan` 的 pending trial、active 与 fallback 选择；
- copy-to-RAM / XIP load plan、handoff、rollback prepare、jump mock 与 confirm；
- bad entry、签名与 policy 相关拒绝路径；
- ARMv7-A load/exec、interrupt、exception、trap 与 runtime bridge 契约检查。

最终以 `[boot] ok=1` 汇总全部检查。该 token 只证明本 fixture 的 host 结果，不证明真实板机器
状态、Flash 断电一致性或产品 bootloader。

X/YMODEM 子集见 [`bootloader_xymodem.md`](../../../docs/boot/bootloader_xymodem.md)，ARMv7-A
边界见 [`armv7a_platform_contract.md`](../../../docs/system/armv7a_platform_contract.md)。
