# charm-block-device

用途：
- 用于 block.device 适配与 VFS 挂载链路的落地检查。

适用场景：
- SDMMC / SPI Flash / RAM block 的 block.device 适配
- block.registry 注册与 VFS mount

不适用场景：
- 文件系统内部实现优化

依赖规则：
- `../../rules/charm-architecture.md`
- `../../rules/embedded-modern-cpp.md`

---

## 工作流程

1. 明确介质类型与 block.device 边界（读/写/擦除/对齐）
2. 完成适配层（介质 -> block.device）
3. 注册到 block.registry（能力入口唯一）
4. 通过 VFS 挂载（只依赖 block.device）
5. 做最小验证（mount/open/read/write）

---

## 产出要求

- 适配层职责边界说明
- registry/mount 入口说明
- 最小验证路径（含错误路径）

---

## 检查要点

- 是否绕过 block.device 直连介质
- 是否在上层引入平台细节
- 是否有最小读写验证链
