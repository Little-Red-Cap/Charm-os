# Boot 示例入口

本目录收纳 bootloader 主线的最小示例。

当前可先看：

- [`bootloader_demo/README.md`](bootloader_demo/README.md)

如果你还没先看系统侧文档，建议先回到：

- [`../../docs/boot/README.md`](../../docs/boot/README.md)
- [`../../docs/system/armv7a_platform_contract.md`](../../docs/system/armv7a_platform_contract.md)

## 当前示例

### `bootloader_demo`

这是当前 boot 主线的最小验证示例，适合看：

- 下载到 Slot B
- 校验
- 生成 BootPlan
- 回滚预备
- 标记成功

## 使用提醒

- Boot 这条线更偏 Stage1/Stage2 和启动链路，不要和板级 bring-up 文档混成一层。
