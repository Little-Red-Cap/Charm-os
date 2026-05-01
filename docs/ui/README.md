# UI 文档入口

本目录收纳 Charm UI 语义、布局、渲染、热键和多后端规划相关材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按任务进入。

## 按任务进入

### 我想看 UI 的现行硬规则

先读：

- [`ui_kernel_contract.md`](ui_kernel_contract.md)

### 我想看 UI 的结构化视图与状态表达

先读：

- [`structured_view_model_v1.md`](structured_view_model_v1.md)
- [`vivid_widget_state_observe.md`](vivid_widget_state_observe.md)
- `Examples/ui/vivid/list_row_flags_demo`

### 我想看 Vivid 这条线

建议顺序：

1. [`vivid_runtime_charter.md`](vivid_runtime_charter.md)
2. [`vivid_multibackend_plan.md`](vivid_multibackend_plan.md)
3. [`vivid_replay_workflow.md`](vivid_replay_workflow.md)
4. [`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md)
5. [`vivid_display_hotkeys.md`](vivid_display_hotkeys.md)
6. [`vivid_page_layer_style_patch.md`](vivid_page_layer_style_patch.md)

### 我想看 EInk / Player UI

读：

- [`eink_refresh_policy.md`](eink_refresh_policy.md)
- [`player_ui.md`](player_ui.md)
- [`player_vivid_patterns.md`](player_vivid_patterns.md)

## 当前建议阅读顺序

- UI 硬规则：`ui_kernel_contract.md`
- 结构化视图：`structured_view_model_v1.md`
- Vivid 路线：`vivid_runtime_charter.md` → `vivid_multibackend_plan.md` → `vivid_replay_workflow.md` → `vivid_layer_runtime_v0.md`
- Player/UI 组合：`player_ui.md` → `player_vivid_patterns.md`

## 使用提醒

- UI 这条线既有契约，也有项目化样例和阶段性补丁，不要把它们混成一层。
- 当输入、状态提交、布局影响位、渲染 record/execute 边界变化时，应同步更新这里的入口。
