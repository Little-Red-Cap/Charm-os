# init Recipe / Plan 草案

本文档用于约束 `init.graph` 之上的新装配表面。

目标不是替代 `init.graph`，而是把接入者的心智模型从“手写 `Node`”提升为“声明 `Recipe`、组合 `Plan`、交给 `Materializer` 落成 IR”。

## 总纲

- `Plan` 继承约束，不继承产出。
- `Capability` 表达可用性与顺序，不表达对象。
- `Recipe` 描述组件，`Barrier` 导出阶段，`Node` 只是 IR。

## 四层对象模型

### 1. `Driver Core`

只表达设备行为与最小 contract：

- 不依赖 HAL
- 不依赖 `platform.board`
- 不依赖 `init.node`
- 不直接声明 capability

### 2. `Binding`

只表达板级对象如何喂给 core：

- 可以依赖 HAL / GPIO / IRQ / DMA / clock
- 不声明 phase / runlevel / capability
- 不负责 graph 装配

### 3. `Recipe`

静态描述一个组件“本来是什么”：

- `name`
- `phase`
- `runlevel`
- `intrinsic requires`
- `intrinsic provides`
- `runtime_type`
- `start / stop`

### 4. `Plan`

描述“这次系统装配怎么约束这些组件”：

- `compose(...)`
- `after(...)`
- `phase_limit(...)`
- `runlevel(...)`
- 未来可扩展 `budget / trace / debug`

## 三层分离

`Recipe` 必须拆成三层：

- `recipe_desc`：静态声明
- `bound_recipe`：绑定 runtime 后的实例
- `Node`：materialize 后的 IR

禁止把这三层揉在一起。

## Barrier 规则

`Plan` 不负责隐式 `provides`。

如果需要把某个子树“完成”提升成 capability，必须插入显式 barrier：

- 语义层：子树导出 capability = 插入一个 barrier node
- 语法层：允许 `ready_as<Cap>(plan)` 作为 barrier 的语法糖

但 `ready_as<Cap>(plan)` 只能降解成 barrier node，不能回退成“向子树所有节点扩散 `provides`”。

## Capability 规则

Capability 只负责：

- 启动顺序
- readiness / 可用性
- 依赖图校验

Capability 不负责：

- 对象发现
- 运行时句柄获取
- service locator 语义

对象与句柄应由 `Binding` / `Runtime` / `Profile` 显式注入。

## 合并规则表

### `requires`

`effective_requires = intrinsic_requires ∪ inherited_requires`

### `provides`

只看 `Recipe` 自己声明。

- `Plan` 不继承 `provides`
- 子树导出必须走 `Barrier`
- `ready_as(...)` 仅允许包装“所有叶子都显式提供 capability”的子树，否则 `materialize(...)` 直接报错
- `export_as(...)` 仅保留为兼容别名；对外推荐表面统一为 `ready_as(...)` 或显式 barrier

### `runlevel`

父 `Plan` 是约束上限。

- 规则：取交集 / clamp

### `phase_limit`

父 `Plan` 是最大 phase。

- 子 `Recipe` 超出时，`materialize` 直接报错

### `budget / trace / debug`

父 `Plan` 向子树继承。

## Materializer 职责

`Materializer` 只负责四件事：

1. 展开 `Plan`
2. 合并 inherited constraints
3. 做 provider 冲突 / 缺失依赖 / phase / runlevel / 上限检查
4. 生成 `Node[]` 与 `const Node*[]` 喂给现有 `Graph`

`Graph` 内核保持不变。

## 当前实现约定

当前代码中的最小接口已经落在：

- `Modules/init/init.meta.cppm`
- `Modules/init/init.recipe.cppm`
- `Modules/init/init.plan.cppm`
- `Modules/init/init.barrier.cppm`
- `Modules/init/init.materialize.cppm`

当前迁移原则：

- 旧式 `node_span()` / `Node* span` 不再作为装配入口；接入对象统一收敛到 `plan()`、`as_plan(...)`、`maybe(...)`
- 新增装配代码优先写成 `Recipe + Plan`
- 业务/驱动接入层不直接写 `init::Node`
- 框架级 bringup helper 统一暴露 `start_plan(...)`，不再保留 `extra_nodes` 形态
- `wrap_nodes_with_requires(...)` 这类 raw-node 过渡接口已退出主路；遗留链路先补齐 `plan()` 再接入
- 框架内的 `*Chain` / `CoreSystemChain` 现在默认只暴露 `plan()`
- multi-node 组件通过 `compose(...)` 组织，而不是继续暴露 legacy 遍历协议

## 当前迁移状态

当前已经有几处真实入口切到了这条新路径：

- `Examples/project/player/profiles/hqzy_cm7_usb_self_msc.system.cppm`
- `Examples/project/player/stn32h747_HQZY/CM7/app/app_pre_bringup.cppm`
- `Examples/project/player/stn32h747_HQZY/CM7/app/app_post_bringup.cppm`
- `Examples/project/player/stn32h747_HQZY/CM7/app/main.cpp`
- `Examples/fs/fs_block_vfs_demo/main.cpp`
- `Examples/usb/usb_msc_block_demo/main.cpp`
- `Modules/system/bringup/system_bringup.cppm`
- `Modules/system/bringup/system_bringup_block.cppm`
- `Modules/system/bringup/system_bringup_input.cppm`
- `Modules/system/bringup/system_bringup_console.cppm`
- `Modules/system/bringup/system_bringup_win_stub.cppm`

这几处目前统一采用：

- `compose(...)` 组织装配树
- `as_plan(...)` 负责单节点 binding；`maybe(...)` 负责可选装配单元；`chain.plan()` / `compose(...)` 负责多节点装配
- 单节点 binding 通过 `as_plan(...)` 直接落成单节点装配项
- 框架内默认组合表面优先用 `chain.plan()`
- `materialize(...)` 只识别新的 `Plan` 主路径，旧 raw-node compat 与 legacy chain 桥接已退出主路
- `runlevel(...)` / `phase_limit(...)` 施加继承约束
- `materialize(...)` 落成旧 `Graph` 需要的 `Node` IR
- `Graph::build(...)` 使用 `materialized_graph` 给出的有效 `runlevel/phase` 过滤参数，避免双重语义源
- 推荐用 `build_graph(...)` / `start_graph(...)` 作为 `Plan -> Graph` 的默认胶水层，避免重复样板代码
- 旧式 `chain.build()` 也优先并到 `build_graph(...)`，避免框架内部继续分叉出第二套落地路径
