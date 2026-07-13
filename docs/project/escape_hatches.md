# Escape Hatch 审查规则

## 文档状态

- `status`: `supporting`
- `scope`: 有意绕过常规抽象或类型边界的局部实现
- `authority`: source marker、benchmark 与平台/ABI 证据

Escape hatch 必须在目标代码附近使用 `ESCAPE_HATCH` 标记，并说明原因。当前实例从源码枚举，
不在本文维护容易漂移的登记表：

```powershell
git grep -n ESCAPE_HATCH -- Modules Examples targets
```

Review 时至少核对：替代方案为何不足、性能/平台/ABI 证据、影响范围、失败风险和移除条件。
没有 benchmark 或硬件约束证据时，不能用“性能关键”作为长期例外理由。

该标记不授予跨模块 API、Core 身份或永久兼容承诺；证据失效后应删除例外。
