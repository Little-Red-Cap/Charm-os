# Charm Capability MVP QEMU

## 文档状态

- `status`: `supporting`
- `scope`: QEMU `virt` 上的 portable MVP 局部运行证据
- `authority`: [`CONSTITUTION.md`](../../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../../docs/architecture/charm_core_contract.md)

本目录以 `arm-none-eabi-g++` 构建 freestanding ELF，并在 QEMU `virt/cortex-a15` 运行。固件直接包含
上级 [`mvp_app.hpp`](../mvp_app.hpp)；PL011、启动、链接和内存 BlockDevice 只属于 QEMU 支持层。

运行：

```powershell
.\Examples\system\charm_capability_mvp\qemu\run_qemu_ci.ps1
```

验证范围：

- 正例得到与 Host 相同的 timestamp `424242` 和 checksum `0x49b880f0`；
- missing binding 分类为 `missing_binding`，应用启动次数为 `0`；
- QEMU 以 semihosting 状态码退出，runner 设有超时。

该结果不覆盖完整 Host failure matrix、真实板、真实时钟或持久存储。
