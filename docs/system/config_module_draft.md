# Config Module 草案

本文用于收敛 Charm-os 的配置表面，目标是把“构建宏输入”逐步压缩成“类型化配置模块”。

## 总纲

- `Config Module` 是语义配置，不是预处理器开关堆
- 宏优先留在构建边界，不直穿业务逻辑
- 业务代码优先读取 `config::...`、`kConfig...`、`if constexpr (...)`
- `configure_file(...)` 生成的是实现细节，公开模块名应尽量稳定

## 当前 MVP

当前已经有两条样板链：

- `player` app config
  - 稳定表面：`Examples/project/player/stn32h747_HQZY/CM7/app/app_config.cppm`
  - 生成实现：`Examples/project/player/stn32h747_HQZY/CM7/cmake/player_app_config.generated.cppm.in`
  - CMake 接线：`Examples/project/player/stn32h747_HQZY/CM7/cmake/product_player_config_module.cmake`
  - 当前分层：`ProductConfig / BoardDefaultsConfig / AppBehaviorConfig`，同时保留 `kIdentity / kBringup / kDebug` 等兼容别名
- `vivid` core config
  - 稳定表面：`Modules/ui/vivid/core/config.cppm`
  - 生成实现：`Modules/ui/vivid/cmake/config.generated.cppm.in`
  - CMake 接线：`Modules/ui/vivid/vivid.cmake`

实现原则：

- 公开模块 `player.stm32h7.app_config` 保持稳定
- 生成模块 `player.stm32h7.app_config.generated` 承载 CMake 输入
- `player` 允许先在生成实现里推进 `product / board / app` 语义分层，再由 `app_config` 继续聚合导出
- 消费侧优先读取 `kProduct / kBoard / kApp` 这类 typed 主表面；`kBringup / kDebug / kRxCap` 等别名仅作为迁移兼容层
- 公开模块 `charm.core.config` 保持稳定
- 生成模块 `charm.core.config.generated` 承载 `vivid` 的屏幕 / feature / SoA 配置输入
- 生成模块同时导出 typed struct 与兼容常量别名，降低迁移摩擦

## 推荐迁移顺序

### 第一类：最先迁移

- profile / scenario 选择
- buffer / queue / arena / pool cap
- 板级业务开关，例如 `enable_audio`、`enable_usb_msc`
- 调试策略开关，例如 `debug stop`、`trace budget`

### 第二类：保留宏，但只留在边界

- C / ASM / vendor SDK 必须依赖的编译定义
- 目标是否参与编译的开关
- 编译器 / 平台探测

### 第三类：后续继续推进

- `vivid` 的屏幕与 feature 配置
- 根 `CMakeLists.txt` 的 feature 宏镜像模块
- `Profile Package` 下的 board / app / product config 统一入口

## 落地规则

- 新增可语义化的配置，优先加到生成模块，不优先加宏
- 若必须保留宏，同步考虑是否导出 typed mirror 值
- 公开模块应稳定，生成实现允许随 CMake 演进
- `Config Module` 优先按领域分层：`board`、`app`、`product`，不要做全局大杂烩
