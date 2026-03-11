# charm-capability-map

用途：
- 用于维护 `docs/capability_map.md` 的能力索引与命名规则。

适用场景：
- 新增/调整 Capability
- 更新状态与文档入口

不适用场景：
- 纯实现细节变更

依赖规则：
- `../../rules/charm-architecture.md`

---

## 维护规则

- Group 固定为 8 个（Core/System/IO/Storage/USB/UI/Media/Platform）
- Capability 命名：PascalCase、单数、名词化
- Status 仅使用：stable/draft/planned/internal
- Docs/Example 缺失时写 `—`

---

## 输出要求

- 更新后的能力表行
- 关联的 Docs/Example/Status 变更说明
