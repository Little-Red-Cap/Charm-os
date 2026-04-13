# Config Module 草案

本文用于收敛 Charm-os 的配置表面，目标是把“构建宏输入”逐步压缩成“类型化配置模块”，并为 `Profile Package` 提供稳定、可组合的语义配置入口。

## 总纲

- `Config Module` 是语义配置，不是预处理器开关堆
- 构建系统拥有配置值，源码读取类型化结果，不再直接消费散装宏
- 公开模块保持稳定，生成模块承载实现细节，二者职责分离
- 消费侧优先读取 typed 主表面；兼容别名和兼容壳只服务迁移，不再作为事实源

## 核心规则

### 1. 稳定公开表面 + 生成实现模块

- 每条配置链都应至少分成两层：稳定公开模块、生成实现模块
- 稳定公开模块负责提供长期可依赖的 import 名称，例如 `player.stm32h7.app_config`、`player.stm32h7.board_config`、`charm.core.config`
- 生成实现模块负责镜像 CMake 输入，例如 `*.generated` 模块；它可以随构建脚本演进，但不应直接成为业务侧默认 import 名称
- 公开模块可以做轻量聚合与兼容导出，但不应再维护一份独立硬编码事实源

### 2. typed 主表面优先

- 每条配置链都应提供显式 typed 主表面，而不是只导出零散常量
- `player` app config 目前以 `kProduct / kBoard / kApp` 为主表面
- `player` board config 目前以 `kConfig / kSdram / kSdmmc / kKey` 为主表面
- `vivid` core config 目前以 `VividConfig` 及相关 typed 配置表为主表面
- 新代码优先直接读取这些 typed 表面；只有迁移期代码才应继续读取叶子常量别名

### 3. 兼容别名是过渡层，不是主语义

- 旧常量名、旧字段名、旧模块名允许暂时保留，但应明确视为兼容层
- 兼容别名应尽量显式标注 deprecated，并指向新的 typed 主表面
- 新增配置时，不应再先设计一组宏或平铺常量，再额外补 typed 封装；正确顺序应反过来
- 评审时若出现“新功能继续直接读旧别名”，应优先改为读取 typed 主表面

### 4. 兼容壳只转发，不拥有事实源

- 旧模块名若仍需保留，应降为 compat shell，只 import 新主模块并 re-export 兼容符号
- compat shell 不再承载独立硬编码事实，也不再复制一份配置值
- `player` 的 `board.active` / `board.hqzy` 当前就属于这种 compat shell：事实源已经回收到 `player.stm32h7.board_config`
- 类似 `vivid_features.generated.hpp` 这类头文件也应视为预处理兼容桥，而不是新配置语义的最终归宿

### 5. 构建系统拥有输入，源码拥有语义

- CMake cache、toolchain 选择、board/profile 选择等仍属于构建输入
- `configure_file(...)`、生成 `.cppm`、稳定 façade 模块，负责把这些输入转换成源码可读的 typed 语义
- 对于可类型化的业务配置，不再优先使用 `target_compile_definitions(...)`
- 宏优先保留在三类边界：`C / ASM / vendor SDK` 依赖、目标是否参与编译、编译器/平台探测

## 当前样板链

### `player` app config

- 稳定表面：`Examples/project/player/stn32h747_HQZY/CM7/app/app_config.cppm`
- 生成实现：`Examples/project/player/stn32h747_HQZY/CM7/cmake/player_app_config.generated.cppm.in`
- CMake 接线：`Examples/project/player/stn32h747_HQZY/CM7/cmake/product_player_config_module.cmake`
- 当前分层：`ProductConfig / BoardDefaultsConfig / AppBehaviorConfig`
- 当前主表面：`kProduct / kBoard / kApp`
- 当前兼容层：`kIdentity / kBringup / kDebug` 等旧别名，已退成显式迁移层

### `player` board config

- 稳定表面：`Examples/project/player/bsp/board_config.cppm`
- 生成实现：`Examples/project/player/stn32h747_HQZY/CM7/cmake/player_board_config.generated.cppm.in`
- CMake 接线：`Examples/project/player/stn32h747_HQZY/CM7/cmake/product_player_config_module.cmake`
- 当前形态：`BoardConfig / SdramConfig / SdmmcConfig / KeyConfig`
- 当前主表面：`kConfig / kSdram / kSdmmc / kKey`
- 当前兼容层：叶子常量别名仍保留，但已明确退成兼容层
- 当前 compat shell：`Examples/project/player/bsp/board_active.cppm`、`Examples/project/player/bsp/board_hqzy.cppm`

### `vivid` core config

- 稳定表面：`Modules/ui/vivid/core/config.cppm`
- 生成实现：`Modules/ui/vivid/cmake/config.generated.cppm.in`
- CMake 接线：`Modules/ui/vivid/vivid.cmake`
- 当前主表面：`VividConfig`、屏幕配置、SoA 配置、feature 表
- 当前兼容层：兼容常量别名与 `vivid_features.generated.hpp` 这类预处理桥接层

## 迁移顺序

### 第一类：优先迁移到 Config Module

- profile / scenario / product identity 选择
- buffer / queue / arena / pool cap
- 板级业务开关，例如 `enable_audio`、`enable_usb_msc`
- 调试策略开关，例如 `debug stop`、`trace budget`
- UI / feature 裁剪开关，只要它们属于业务语义而不是编译器探测

### 第二类：保留宏，但只留在边界

- `C / ASM / vendor SDK` 必须依赖的编译定义
- 控制某目标或某源码是否参与编译的开关
- 编译器 / 平台 / 架构探测宏

### 第三类：在样板稳定后继续推进

- `Profile Package` 下的 `product / board / app` 统一配置入口
- 根 `CMakeLists.txt` 层面的 feature 宏镜像模块
- 更多 compat shell 与 compat alias 的清退

## 评审准则

- 新增配置若能被类型化，应优先进入生成模块与 typed 主表面，而不是新增 compile definition
- 新增消费侧代码应优先读取 `kProduct / kBoard / kApp`、`kConfig / kSdmmc / kSdram / kKey`、`VividConfig` 等主表面
- 若仍需保留旧模块名或旧常量名，必须明确其 compat 身份，并让事实源回收到新主模块
- 若发现 compat shell 内重新出现硬编码事实，视为回退，应在评审中拦下
- 文档、CMake、生成模板、公开 façade 应一起演进，避免“构建入口已经迁移，源码表面仍停留旧语义”

## 当前阶段判断

- `player` 已经具备 app config 与 board config 两条生成式样板链
- `vivid` 已经具备屏幕 / feature / SoA 的生成式 Config Module 样板链
- 下一阶段重点不是继续扩散兼容层，而是让更多消费侧直接读取 typed 主表面，让 compat alias 真正退成过渡层
- 当 `player` 与 `vivid` 两条链都稳定后，再进一步归纳 `product / board / app` 的统一规范会更稳
