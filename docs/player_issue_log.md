# Player 示例问题与改进记录

> 目的：记录在推进 Player/vivid 示例过程中遇到的小问题与可改进点，方便后续统一处理。

## 已遇到的问题（已做临时修复）

1) `Modules/core/alg/alg_dither.cppm` 缺少 `<span>` include
- 现象：全量模块编译时报 `span` 未声明
- 临时修复：补 `#include <span>`
- 建议：在模块自检或 CI 中加入“缺头文件”检测

2) `Modules/io/input/input.raw_event.cppm` 未 re-export `input.raw`
- 现象：依赖 `input::Button` 的模块无法编译
- 临时修复：`export import input.raw;`
- 建议：明确 raw_event 的导出契约并补测试

3) `Modules/ui/vivid/font/typography.cppm` 漏 import `lv_font_montserrat_12`
- 现象：`font_montserrat_12` 未定义
- 临时修复：补 `import charm.font.lv_font_montserrat_12;`
- 建议：字体注册统一入口/生成脚本校验

4) `Modules/io/out/out.port.template.cpp` 缺少基础 include/import
- 现象：`std::size_t`/`std::unexpected` 等未识别
- 临时修复：补 `<cstddef>`、`<expected>`、`import out.core;`
- 建议：平台端口模板应可直接编译通过

5) `AudioPlayer::seek_ms` 仅支持 WAV
- 现象：FLAC/MP3 返回 `Errc::not_supported`，示例里只能提示“不支持 seek”
- 临时处理：示例限制拖拽仅在播放期触发 seek，失败则回滚
- 建议：补 FLAC/MP3 的 seek 能力或提供统一的“是否可 seek”查询

6) `charm.core.pool` 缺少 `<cstddef>` 引用
- 现象：`std::size_t` 未声明导致模块编译失败
- 临时修复：在模块全局片段补 `#include <cstddef>`
- 建议：基础容器模块做一次 “头文件自检”

7) `UiFactory::make_handle` 模板推导失败
- 现象：`make_handle(containers_.create(), ...)` 无法推导 `Pool`
- 临时修复：改为 `template <typename Handle> make_handle(std::optional<Handle>, ...)`
- 建议：避免在非推导上下文中使用 `Pool::Handle`

8) `Gui` 使用 `trace::TraceKind` 未导入命名空间
- 现象：`trace` 未声明
- 临时修复：改为 `service::TraceKind::counter`
- 建议：统一 trace 的导出与命名约定

9) `gui.trace`/`input.trace` 的导出常量与命名冲突
- 现象：`TraceKind` 在 `gui::trace`/`input::trace` 命名空间内被遮蔽，且 `static constexpr` 导致导出报错
- 临时修复：使用 `service::TraceKind::*`，并将常量改为 `inline constexpr`
- 建议：trace 子模块命名与全局 `trace` 保持区分，避免二义性

## 架构改进建议（待你确认推进方向）

1) VFS 桥接缺口
- 现状：RAMFS/BlockFS 可用，但 PC 端“file-backed block device + FATFS”仍缺
- 进展：已新增 file-backed block device + FatFs 适配与示例
- 参考：`Examples/fs/vsf_fs_fatfs_demo`

2) vivid 屏幕尺寸配置
- 现状：固定 1280x720
- 已临时加入宏 `CHARM_VIVID_SCREEN_WIDTH/HEIGHT`
- 建议：正式化为 config 层接口或 build 选项

3) 示例构建策略
- 全量 `Modules/**/*.cppm` 容易拉入无关模块，放大编译/链接问题
- 建议：示例层提供可配置“模块包”清单或预设组合
