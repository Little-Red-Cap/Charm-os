# SSU 纪律（阶段 1）

## 目标

从这一阶段开始，Charm 不再把 SSU 仅仅视为一种可选描述能力，
而是把它提升为 scheduler task 的默认系统语言。

第一阶段不追求一次性强制全仓报错，而是先建立如下纪律：

- 进入 scheduler 的 task，应该声明 `ssu_meta()`
- task 如果暂时没有 `ssu_meta()`，必须被视为待迁移对象
- 新增 task 默认要求补齐 SSU 语义声明

## 为什么要这样做

仅约束入口并不足以阻止系统继续长出旁路。
真正导致旁路滋生的根因是：系统需要的是“执行语义”，
而不是某个 import 路径。

因此，Charm 必须让“谁在什么上下文下被什么触发、每次做多少、能否阻塞”
成为 task 的默认声明，而不是散落在文档、注释和约定里的隐性知识。

## 阶段 1 规则

### 规则 1：scheduler task 应声明 `ssu_meta()`

推荐所有进入 `kernel::TaskRegistry` 的 task 都定义：

```cpp
static consteval kernel::ssu::Meta ssu_meta() noexcept;
```

### 规则 2：默认使用温和约束

第一阶段默认不因为缺失 `ssu_meta()` 让全仓编译失败，
但内核必须提供一个可切换的编译期开关，允许在收敛阶段升级为严格模式。

### 规则 3：新 task 视为必须声明 SSU

后续新增 task 时，review 默认要求补齐 `ssu_meta()`。
如果确实暂时无法声明，必须说明原因和迁移计划。

## 严格模式

内核提供如下开关：

- `CHARM_KERNEL_REQUIRE_SSU_META=1`

启用后，`TaskRegistry` 会对缺失 `ssu_meta()` 的 task 触发编译期失败。

## 第一阶段不做的事

- 不要求所有 task 立刻迁完
- 不要求所有 data plane 都改写成 SSU
- 不要求 scheduler 改成按 SSU 执行
- 不把 SSU 变成 runtime 反射系统

## 第一阶段完成标志

- 核心 pump/task 已补齐 `ssu_meta()`
- `tasks.json` 可见 SSU 标签
- `trace.json/csv` 可见 SSU 标签
- 内核具备严格模式开关
