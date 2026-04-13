# init.materialize 观察导出面

本文档说明 `materialized_graph` 的最小工具消费观察面。

它回答的不是“系统如何执行”，而是“`Plan` 在 `materialize(...)` 之后，哪些规范化结果可以被稳定观察、导出和工具消费”。

## 目标

当前这层观察面的目标很收敛：

- 基于 `materialized_graph`，而不是输入阶段的 `Plan` 语法树
- 只暴露只读、语义化视图，不把裸 `Node` 内存布局承诺给工具
- 先服务结构可见性与工具消费验证，不反向绑架 DSL 或 `Graph` 内核
- 先支持 `DOT` 与 `JSON sample` 导出，再根据实际工具需求收敛长期协议

这意味着它是一扇开在 `Materializer` 之后的观察窗，而不是新的执行后端。

## 挂点

当前实现位于：

- `Modules/init/init.materialize.cppm`
- `Modules/init/init.observe.cppm`

其中：

- `materialize(...)` 负责把 `Plan` 归一化为 `materialized_graph`
- `observe(...)` 负责把 `materialized_graph` 投影成只读语义视图
- `format_dot(...)` 和 `format_json_sample(...)` 负责最小样例导出

## 当前暴露的视图

`init.observe` 当前导出三类只读 DTO：

- `materialized_node_view`
- `materialized_edge_view`
- `materialized_graph_view`

### `materialized_node_view`

每个节点当前可稳定观察到：

- `index`
- `name`
- `phase`
- `runlevel_mask`
- `provides`
- `provide_names`
- `requires_caps`
- `require_names`
- `kind`

其中 `kind` 当前分为：

- `recipe`
- `barrier`
- `legacy`

### `materialized_edge_view`

边不是输入语法的一部分，而是观察层根据规范化结果推导出的依赖关系。

当前每条边包含：

- `provider_index`
- `consumer_index`
- `capability`

边的生成规则是：对每个节点的 `requires`，在当前 `materialized_graph` 中找到唯一 provider。

## capability 名称

当前实现已经支持把 type-level capability 名称带到观察导出层。

对于由 `Recipe` 和 `ready_as<Cap>()` 产生的 capability：

- `materialize(...)` 会同时保留 `CapId` 和 `name`
- `DOT` 导出会优先显示 `name (0x...)`
- `JSON sample` 会导出 `{ "id": "0x...", "name": "..." }`

对应实现可见：

- `Modules/init/init.meta.cppm`
- `Modules/init/init.materialize.cppm`
- `Modules/init/init.observe.cppm`

### 当前边界

兼容层的 legacy node 仍然只以旧 `Node` IR 为输入。

旧 `Node` 只携带：

- `provides: span<CapId>`
- `requires_caps: span<CapId>`

因此：

- 如果 capability 来源是 `Recipe` / `ready_as<Cap>()`，通常能恢复名称
- 如果 capability 来自原始 legacy node，且旧输入没有额外名称信息，则当前只能回退为十六进制 `CapId`

这是当前 compat 边界，不是观察 API 的 bug。

### legacy 名称恢复

当前 compat 路径已经支持两种最小命名恢复方式。

#### 1. 对象级 hook

如果通过 `as_plan(...)`、`legacy(...)`、`maybe(...)` 等路径接入的 legacy 对象实现：

```cpp
std::string_view capability_name(init::CapId id) const noexcept;
```

那么 `materialize(...)` 会在落图时用它补全该对象相关节点的 capability 名称。

这适合：

- 单节点 legacy binding
- 暴露 `for_each_legacy_node(...)` 的旧 chain
- 暴露 `node_span()` 的旧 chain

#### 2. raw span 名称表

如果调用方只有裸 `Node*` span，可以使用：

```cpp
init::compat_nodes(nodes, capability_names)
```

其中 `capability_names` 是 `std::span<const init::cap_name_entry>`。

`cap_name_entry` 当前形态为：

```cpp
struct cap_name_entry {
    CapId id;
    std::string_view name;
};
```

这适合：

- bringup 兼容入口临时带名字表
- 历史 `node_span()` 世界做最小观察增强
- 不想改 legacy `Node` IR 本体时的过渡接入

## DOT 导出

`format_dot(...)` 适合做第一阶段结构观察。

当前导出内容包括：

- 全图 `phase` / `runlevel` / `node_count` / `edge_count`
- 每个节点的 `name` / `kind` / `phase` / `runlevel`
- 每个节点的 `provides` / `requires`
- 每条依赖边的 capability 标签

节点形状当前约定为：

- `recipe` -> `box`
- `legacy` -> `ellipse`
- `barrier` -> `diamond`

这层输出的目标不是审美，而是尽快验证：

- barrier 是否插对
- 依赖边是否合理
- phase/runlevel 是否被规范化到预期结果

## JSON sample 导出

`format_json_sample(...)` 当前导出的是样例协议，而不是长期冻结协议。

当前 schema 标识为：

- `materialized_graph.sample/v2`

`v2` 的变化重点是 capability 从纯字符串升级成对象：

- `{"id":"0x...","name":"demo.clock"}`

JSON sample 当前更适合：

- 脚本分析
- IDE 原型接入
- 工具链字段勘探
- 后续长期 schema 设计前的验证

不建议现在就把 `sample/v2` 当作最终稳定协议承诺给外部工具。

## 最小用法

### 代码层

最小调用方式：

```cpp
auto mats = init::materialize<MaxNodes, MaxCaps>(plan_value);
auto view = init::observe(*mats);

std::array<char, 8192> dot{};
std::array<char, 8192> json{};

auto dot_bytes = init::format_dot(view, dot.data(), dot.size());
auto json_bytes = init::format_json_sample(view, json.data(), json.size());
```

也可以直接对 `materialized_graph` 调用：

```cpp
auto dot_bytes = init::format_dot(*mats, dot.data(), dot.size());
auto json_bytes = init::format_json_sample(*mats, json.data(), json.size());
```

### 示例工程

仓库中已经提供两个示例：

- `Examples/init/materialize_observe_demo/main.cpp`
- `Examples/init/bringup_block_observe_demo/main.cpp`
- `Examples/init/bringup_minimal_observe_demo/main.cpp`

这个示例刻意同时包含：

- `recipe` 节点
- `legacy` 节点
- `ready_as<Cap>()` 产生的 `barrier` 节点

并且示范了 legacy 节点通过 `capability_name(...)` 恢复 capability 可读名称。

便于直接观察三类节点在导出中的表现。

其中 `bringup_block_observe_demo` 走的是更贴近真实 bringup 的组合：

- `BringupBlock`
- `FileInitChain`

它用来验证：bringup helper 组合出的系统装配结果，也可以在 `start_graph(...)` 之前先被 materialize、观察和导出。

`bringup_minimal_observe_demo` 则进一步把这件事推进到更接近系统入口的层面：

- `BringupMinimal`
- `BoardCaps`
- board bringup 中的 console alias / input / can 组合

它用来验证：系统入口 helper 本身，也可以直接说出“系统长什么样”。

## 构建与运行示例

推荐使用 Clang 单独配置示例目录：

```powershell
cmake -S Examples/init/materialize_observe_demo -B cmake-build-init-observe-demo-clang -G Ninja -DCMAKE_C_COMPILER=<clang> -DCMAKE_CXX_COMPILER=<clang++>
cmake --build cmake-build-init-observe-demo-clang -j 8
```

如果只是想一键产出默认文件，也可以直接走示例内建目标：

```powershell
cmake --build cmake-build-init-observe-demo-clang --target export_materialized_graph_demo -j 8
```

然后运行：

```powershell
./init-materialize-observe-demo.exe
```

默认会在当前工作目录生成：

- `materialized_graph.dot`
- `materialized_graph.sample.json`

也可以显式指定输出路径：

```powershell
./init-materialize-observe-demo.exe --dot out.dot --json out.json
```

仓库根目录还提供了封装脚本：

```powershell
./scripts/export_materialized_graph.ps1
```

它默认会：

- 配置 `Examples/init/materialize_observe_demo`
- 使用 `cmake-build-init-observe-demo-clang`
- 触发 `export_materialized_graph_demo` 目标

如果要显式指定导出路径：

```powershell
./scripts/export_materialized_graph.ps1 -Dot out.dot -Json out.json
```

如果要查看当前可批量导出的样例列表：

```powershell
./scripts/export_materialized_graph.ps1 -ListCases
```

如果只导出某一个样例：

```powershell
./scripts/export_materialized_graph.ps1 -Case materialize-observe-demo
./scripts/export_materialized_graph.ps1 -Case bringup-block-observe-demo
./scripts/export_materialized_graph.ps1 -Case bringup-minimal-observe-demo
```

如果要一次导出全部已登记样例：

```powershell
./scripts/export_materialized_graph.ps1 -AllCases
```

## 当前验收点

这一轮观察面成立，主要看下面几件事：

- 工具层不需要直接遍历裸 `Node`
- `materialized_graph` 已足够恢复节点、依赖边、phase、runlevel、barrier
- `DOT` 已能快速暴露结构形状
- `JSON sample` 已能给脚本与工具原型提供统一输入
- capability 名称已经可以沿 `Recipe / ready_as` 路径被保留到导出层

## 后续演进方向

当前更合适的后续工作包括：

- 为 legacy compatibility path 补 capability 名称恢复策略
- 设计从 `sample` 走向稳定 schema 的版本化规则
- 增加 profile / build 维度的批量导出入口
- 让导出结果进入 CI、可视化或 IDE 工具链

当前不建议在这一层直接做：

- 反向定义 DSL
- 暴露 `Node` 内部布局作为长期外部协议
- 把 `sample` 格式过早冻结成不可演化的正式标准
