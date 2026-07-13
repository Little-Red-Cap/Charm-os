# IO Route

## 文档状态

- `status`: `supporting`
- `scope`: Channel、Reactor、Registry 与 IO layering 路由

## 最短路径

1. [IO 入口](../../io/README.md)
2. [Channel](../../io/io_channel_contract.md)
3. [Reactor](../../io/io_reactor_contract.md)
4. [Registry](../../io/io_registry_contract.md)

Channel 不等待且不得返回 `Ok(0)`；Reactor 负责事件推进；Registry 只发布 non-owning endpoint。
