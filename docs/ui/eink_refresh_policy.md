# e‑ink Refresh Policy 草案（Vivid 后端）

目标：为 e‑ink 后端提供可配置的刷新策略，兼容 **Tile/PFB + DrawCmd** 的渲染模型。

## 1. 设计原则

- **只刷新变化区域**：默认以 tile 命中表作为 dirty 来源。
- **避免频繁全刷**：全刷频率受限，必要时降级。
- **可预测**：刷新策略必须确定性可配置。
- **后端独立**：UI 与 DrawCmd 不感知刷新策略。

## 2. 刷新模式（最小集合）

```text
FullRefresh        // 全屏刷新（慢、稳定）
PartialRefresh     // 局部刷新（快、残影风险）
Auto               // 基于频率/面积/时间自动切换
```

## 3. 关键策略参数

```text
max_partial_count      // 连续局刷次数上限，超出触发全刷
min_full_interval_ms   // 两次全刷的最小间隔
partial_area_ratio     // 局部刷新面积阈值（超过则全刷）
```

## 4. 典型策略（推荐默认）

```text
mode = Auto
max_partial_count = 20
min_full_interval_ms = 30_000
partial_area_ratio = 0.35
```

执行逻辑（伪码）：

```cpp
if (dirty_area_ratio >= partial_area_ratio)
    do_full_refresh();
else if (partial_count >= max_partial_count)
    do_full_refresh();
else
    do_partial_refresh();
```

## 5. 与 Tile/PFB 的对接

- `DrawCmdExecutor::execute_tiles()` 输出 tile 命中表
- backend 统计 dirty tile → dirty rect → dirty area ratio
- refresh policy 根据 ratio / counter / time 决策

## 6. 透明/抖动注意事项

- e‑ink 仅在 **1bit/2bit 输出**下稳定
- 透明与 alpha 建议在 executor 中 **阈值化/抖动**
- 局刷频率高时，适当降低抖动强度

## 7. 未来扩展

- 多阶段刷新（fast → slow）
- 批量局刷合并窗口
- 设备级 LUT/波形切换

