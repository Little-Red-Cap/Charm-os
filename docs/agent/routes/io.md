# IO Route

## 适用场景

- Channel / Reactor / Registry
- IO 分层与依赖边界
- IO 契约 review / codegen

## 最短阅读顺序

1. [`../../io/README.md`](../../io/README.md)
2. [`../../io/io_channel_contract.md`](../../io/io_channel_contract.md)
3. [`../../io/io_reactor_contract.md`](../../io/io_reactor_contract.md)
4. [`../../io/io_registry_contract.md`](../../io/io_registry_contract.md)
5. [`../skills/charm-io-contracts/SKILL.md`](../skills/charm-io-contracts/SKILL.md)

## 先不要做什么

- 不要引入 busy-spin / 阻塞 / 睡眠式等待。
- 不要让 Channel 返回 `Ok(0)`。
- 不要绕过 registry 直接耦合全局能力。

## 完成前自检

- IO 依赖方向是否正确。
- Channel / Reactor / Registry 契约是否同时满足。
- 行为变更是否同步回写文档。
