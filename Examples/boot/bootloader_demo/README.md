# `bootloader_demo`

这个示例把 bootloader 主线压缩成一个可直接阅读和验证的最小闭环，适合快速确认下载、选槽、handoff 与成功确认这些关键步骤是怎样串起来的。

如果你还没看系统侧背景，建议先读：

- [`../../../docs/boot/README.md`](../../../docs/boot/README.md)
- [`../../../docs/system/armv7a_platform_contract.md`](../../../docs/system/armv7a_platform_contract.md)

## 先看什么

- [`main.cpp`](main.cpp)：示例主体，包含 mock flash、镜像构造、XYMODEM 传输、BootPlan 生成、回滚预备、成功确认，以及 ARMv7A handoff/interrupt 相关契约验证。
- [`CMakeLists.txt`](CMakeLists.txt)：最小构建入口，展示这个示例依赖 Charm 主仓库的方式。

## 这个示例覆盖什么

- 下载镜像到 Slot B。
- 校验镜像头、payload 与签名相关路径。
- 生成 BootPlan，并区分 `pending_trial`、`active`、`fallback` 等选择原因。
- 准备 handoff、执行 boot entry，并在成功后标记 active slot。
- 验证 copy-to-RAM / XIP 两类加载路径。
- 串起 ARMv7A handoff、interrupt、runtime trap 等一组契约级 smoke check。

## 使用提醒

- 这里偏“最小闭环验证”，不是完整产品级 boot 文档。
- 如果要看启动协议、传输方式或文档路由，请回到 [`../README.md`](../README.md) 与 [`../../../docs/boot/README.md`](../../../docs/boot/README.md)。
