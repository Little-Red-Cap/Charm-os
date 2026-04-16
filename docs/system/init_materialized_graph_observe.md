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

当前主路径只保留对象级命名恢复。

#### 对象级 hook

如果通过 `as_plan(...)`、`maybe(...)`，或 `plan()` 内部组合出来的单节点 binding 实现：

```cpp
std::string_view capability_name(init::CapId id) const noexcept;
```

那么 `materialize(...)` 会在落图时用它补全该对象相关节点的 capability 名称。

这适合：

- 单节点 binding
- `maybe(optional_item)` 包裹的单节点 binding
- `plan()` 内部组合出来的 binding 节点

当前仓库里，`init + system bringup` 主路径上常见的 binding 已经逐步补齐这类 hook，例如：

- `system.clock`
- `io.registry` / `block.registry`
- `block.sd0` / `block.flash0` / 其他 block endpoint binding
- `io.reactor` / `kernel.eda` / `system.reactor_pump`
- `platform.irq` / `hal.uart1` / `io.uart1` / `io.console0`
- `input.service` / `input.router` / `input.pump`

因此像 `bringup_block_observe_demo`、`bringup_minimal_observe_demo` 这类更贴近真实 bringup 的导出结果，已经可以直接显示可读 capability 名称，而不只是十六进制 `CapId`。

原先面向 raw `Node*` span 的 `compat_nodes(...) + cap_name_entry` 入口已经移除；观察导出现在只跟随新的 `Plan` 主路径。

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

当前仓库内置的 bundle 消费脚本，已经把 `sample/v2` 当作当前支持的唯一样例协议：

- `scripts/export_materialized_graph.ps1`
- `scripts/inspect_materialized_graph_bundle.ps1`
- `scripts/diff_materialized_graph_bundle.ps1`

这些脚本现在会在读取 case JSON 时显式校验：

- schema 是否受支持
- 当前最小必需字段是否存在

这样做的目的不是提前冻结长期协议，
而是避免工具链在 schema 演进时静默误读旧/新字段。

为了让工具侧有更明确的机器可读锚点，仓库现在还提供了：

- `schemas/materialized_graph.sample.v2.schema.json`
- `schemas/materialized_graph.export_bundle.v1.schema.json`
- `schemas/materialized_graph.bundle_diff.v1.schema.json`
- `schemas/materialized_graph.ci_summary.v1.schema.json`
- `schemas/materialized_graph.report_manifest.v1.schema.json`
- `schemas/README.md`
- `scripts/validate_materialized_graph_artifacts.py`

当前可以这样理解它们的职责边界：

- `sample/v2`：描述当前导出样例的精确形状，但不代表长期冻结承诺
- `export_bundle/v1`：描述 bundle 索引结构，是批量导出 / inspect / diff 的稳定消费面
- `bundle_diff/v1`：描述 diff JSON 结构，是 diff / report / 自动化审阅的稳定消费面
- `ci_summary/v1`：描述 CI 摘要结构，是 workflow / 自动化集成的稳定消费面
- `report_manifest/v1`：描述报告元数据结构，是 HTML / Markdown 报告与上层工具之间的稳定桥接面

如果要把这些协议真正跑成自动验证，仓库现在还提供了：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py --bundle-root out/materialized-graph-bundle
python ./scripts/validate_materialized_graph_artifacts.py --ci-output-root out/materialized-graph-ci
```

它当前会按 `schema` 字段自动选择仓库里的对应 JSON Schema，并验证：

- bundle `index.json`
- bundle 引用的 case `sample.json`
- CI `summary.json`
- `summary.json` 引用到的 `diff.json`
- `summary.json` 引用到的 `report manifest`

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

仓库中已经提供多个示例：

- `Examples/init/materialize_observe_demo/main.cpp`
- `Examples/init/bringup_block_observe_demo/main.cpp`
- `Examples/init/bringup_minimal_observe_demo/main.cpp`
- `Examples/usb/usb_msc_block_demo/main.cpp`

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

`usb_msc_block_demo` 则把观察导出推进到一条更真实的能力链：

- `BringupBlock`
- `FileInitChain`
- `UsbMscBlockInitChain`

它现在支持在 `FakeDcd` + `--export-only` 模式下只导出装配结果，用来观察 block 与 USB MSC class 接入后的归一化图。

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

这些 case 当前来自 `scripts/materialized_graph.export_case_manifest.v1.json`，
对应 schema 为 `schemas/materialized_graph.export_case_manifest.v1.schema.json`。
它承载的是这条导出链当前最小的一组声明式输入事实，
例如 `source / build target / export target / default artifact name / subject`，
但它仍不是最终 `SystemSpec` DSL。
如果只想校验这份输入 manifest 本身，
当前也可以直接运行：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py --export-case-manifest ./scripts/materialized_graph.export_case_manifest.v1.json
```

如果要临时改用另一份 case manifest，
当前也可以显式传入：

```powershell
./scripts/export_materialized_graph.ps1 -ListCases -CaseManifest ./scripts/materialized_graph.export_case_manifest.v1.json
```

如果只导出某一个样例：

```powershell
./scripts/export_materialized_graph.ps1 -Case materialize-observe-demo
./scripts/export_materialized_graph.ps1 -Case bringup-block-observe-demo
./scripts/export_materialized_graph.ps1 -Case bringup-minimal-observe-demo
./scripts/export_materialized_graph.ps1 -Case usb-msc-block-demo
```

如果要一次导出全部已登记样例：

```powershell
./scripts/export_materialized_graph.ps1 -AllCases
```

如果要把多案例结果收拢到一个统一目录，并生成一个供工具消费的索引：

```powershell
./scripts/export_materialized_graph.ps1 -AllCases -OutputRoot out/materialized-graph-bundle
```

这会生成：

- `out/materialized-graph-bundle/index.json`
- `out/materialized-graph-bundle/materialize-observe-demo/materialized_graph.dot`
- `out/materialized-graph-bundle/materialize-observe-demo/materialized_graph.sample.json`
- 以及其它 case 各自的 `DOT / JSON sample`

`index.json` 当前会汇总：

- 这次导出使用的输入 `manifest path / schema`
- case 名称
- 对应 `source / build target / export target`
- case 自带的 `subject` 元数据，例如 `profile / board / active_facets`
- bundle 内相对路径形式的 `dot / json`
- 从 `JSON sample` 提取出的轻量摘要，例如 `node_count / edge_count / phase / runlevel / node_kinds`

这些 `subject` 字段当前不是最终 DSL，
但它们已经可以作为 per-case 的声明式默认事实，
继续被 `artifact report` 与 CI 摘要链自动继承。
更准确地说，
当前 `export case manifest` 负责声明输入侧的 case 事实，
而 `index.json` 负责把这些事实投影到 bundle 消费面。
现在这层投影里也会显式保留输入 `manifest` provenance，
这样 bundle/CI/inspect 不再只能“看到结果”，也能知道“这些结果是基于哪份输入清单生成的”。

仓库根目录还提供了一个最小 bundle 消费脚本：

```powershell
./scripts/inspect_materialized_graph_bundle.ps1 -BundleRoot out/materialized-graph-bundle
./scripts/inspect_materialized_graph_bundle.ps1 -BundleRoot out/materialized-graph-bundle -Case materialize-observe-demo
./scripts/inspect_materialized_graph_bundle.ps1 -BundleRoot out/materialized-graph-bundle -Case materialize-observe-demo -ShowEdges
```

它当前支持：

- 从 `index.json` 汇总所有 case 的 `nodes / edges / phase / runlevel / node_kinds`
- 显示 bundle 顶层记录的输入 `manifest` provenance（如果 index 提供）
- 读取单个 case 的 `JSON sample` 并展开节点表
- 按需展开依赖边表，验证 provider / consumer / capability
- 用 `-AsJson` 把汇总结果重新转成更适合脚本继续消费的结构

如果要比较两份 bundle 的结构差异，仓库根目录还提供了：

```powershell
./scripts/diff_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b
./scripts/diff_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b -Case materialize-observe-demo -ShowDetails
./scripts/diff_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b -AsJson
```

它当前会比较：

- case 是否新增 / 删除 / 变化 / 不变
- case 级摘要字段，例如 `node_count / edge_count / phase / runlevel / node_kinds`
- 节点新增 / 删除 / 字段变化
- 依赖边新增 / 删除

当使用 `-AsJson` 时，当前输出还会带：

- `schema = materialized_graph.bundle_diff/v1`
- `generated_at_utc`
- `include_unchanged`
- `status_counts`

对应机器可读协议见：

- `schemas/materialized_graph.bundle_diff.v1.schema.json`

如果要把 diff 结果进一步交给人审阅，仓库根目录还提供了报告生成脚本：

```powershell
./scripts/report_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b
./scripts/report_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b -Case materialize-observe-demo -OutputDir out/report
./scripts/report_materialized_graph_bundle.ps1 -LeftBundleRoot out/bundle-a -RightBundleRoot out/bundle-b -Format markdown
```

它当前默认会生成：

- `materialized_graph_bundle_diff_report.md`
- `materialized_graph_bundle_diff_report.html`
- `materialized_graph_bundle_diff_report.manifest.json`

报告内容当前包括：

- 左右 bundle 元信息与 case 统计
- case 摘要表
- 每个 case 的 summary changes
- 可点击的 `dot / json` 工件链接
- 节点新增 / 删除 / 字段变化表
- 依赖边新增 / 删除表

其中 `manifest.json` 当前会汇总：

- 左右 bundle 引用
- diff 协议名与 case / status 计数
- Markdown / HTML / manifest 自身路径
- 报告中包含的 case 名单与状态

如果要把这条链收成一个更适合 CI 的单入口，还可以直接用：

```powershell
./scripts/ci_materialized_graph_bundle.ps1 -OutputRoot out/materialized-graph-ci
./scripts/ci_materialized_graph_bundle.ps1 -BaselineBundleRoot out/baseline-bundle -OutputRoot out/materialized-graph-ci
./scripts/ci_materialized_graph_bundle.ps1 -BaselineBundleRoot out/baseline-bundle -Case materialize-observe-demo -FailOnDiff
```

这个入口当前会按顺序做：

- 导出当前工作区的 candidate bundle
- 如果给了 baseline，就生成 `diff.json`
- 继续生成 Markdown / HTML 报告
- 最后产出一个 `summary.json` 供 CI 或上层脚本消费

`summary.json` 当前会汇总：

- 当前运行模式：`export_only / compare`
- candidate / baseline 索引路径
- candidate / baseline bundle 的输入 `manifest` provenance（如果对应 bundle index 提供）
- 是否发现可见差异
- 各类状态计数：`changed / added / removed / unchanged`
- 按状态分组的 case 名单
- 报告与 diff 产物路径

其中 `report` 字段现在也会额外带：

- `manifest`

仓库现在还提供了一个对应的 GitHub Actions 工作流：

- `.github/workflows/materialized-graph-observe.yml`

它当前会：

- 在 `pull_request -> main` 或手动触发时运行
- 分别检出 candidate 与 baseline（PR 基线默认取 base sha，手动触发默认取 `main`）
- 在 baseline 工作树导出一份 bundle
- 在 candidate 工作树执行 `ci_materialized_graph_bundle.ps1` 生成 diff 与 Markdown / HTML 报告
- 上传 `out/materialized-graph-baseline` 与 `out/materialized-graph-ci` 作为 workflow artifact
- 把关键计数与报告路径写入 GitHub Step Summary，方便直接在 Actions 页面浏览

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
