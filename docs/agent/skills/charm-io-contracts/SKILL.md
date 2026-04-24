# charm-io-contracts

用途：
- 用于 IO 相关能力的契约自检与实现落地。
- 重点覆盖 Channel/Reactor/Registry 与非阻塞语义。

适用场景：
- 新增通道适配
- Reactor 事件接入
- Registry 注册与发现路径

不适用场景：
- 纯业务模块改动

依赖规则：
- `../../rules/charm-architecture.md`
- `../../rules/embedded-modern-cpp.md`

---

## 工作流程

1. 明确 IO 能力边界（Channel/Reactor/Registry）
2. 落地 Channel 语义（non-blocking / would_block / 禁止 Ok(0)）
3. 接入 Reactor（notify 入队、回调无 busy-spin）
4. 通过 Registry 统一入口，禁止隐式全局
5. 做最小验证（读/写/事件/错误路径）

---

## 产出要求

- 契约遵循说明（read/write/flush/notify/registry）
- 适配层与边界隔离说明
- 最小验证路径

---

## 检查要点

- Channel read/write 是否 non-blocking
- 资源不足是否统一 `would_block`
- 是否存在 `Ok(0)`
- Reactor 回调是否有自旋/阻塞
- Registry 是否成为唯一入口
