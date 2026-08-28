# Charm Core ARM Build-only 示例

## 文档状态

- `status`: `supporting`
- `scope`: ARM freestanding 工具链对 `Charm::core` 的编译期消费
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

本示例使用 `arm-none-eabi-g++`、freestanding、无异常、无 RTTI 和无线程安全静态初始化选项编译公共关系投影，只生成对象文件。

运行：

```powershell
.\Examples\system\charm_core_arm_build_only\run_build_only.ps1
```

该结果只证明 ARM 工具链可以消费公共 header；不证明链接、启动、QEMU、真实板行为或 MCU 标准库完整性。
