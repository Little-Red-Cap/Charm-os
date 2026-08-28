# Charm Core 安装消费示例

## 文档状态

- `status`: `supporting`
- `scope`: 安装后 `Charm::core` 的 Host 消费边界
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

本示例先安装根 `Charm-core` target，再通过 `find_package(CharmCore CONFIG REQUIRED)` 构建消费者。
消费者不添加仓库源码 include path，也不通过 `add_subdirectory` 接入 producer。

运行：

```powershell
.\Examples\system\charm_core_external_consumer\run_host_ci.ps1 -Profile all
```

该 gate 证明安装头和导出 target 可被独立 Host 工程消费；不证明 ABI 稳定、包版本策略、QEMU 或板级兼容。
