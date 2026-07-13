# 构建模型早期取舍保留笔记

> `status`: `archived`

当前操作入口见 [`build route`](../../agent/routes/build.md)。本文只保留复杂项目构建与目录迁移的通用
边界；真实 target、preset、toolchain 和 source ownership 必须检查当前 CMake。

## Artifact Target

独立 firmware、Host app、QEMU image 或诊断 artifact 应有独立 executable/library target。注释入口、
手改 source list 或一个变量反复改变同一 target，会混淆 IDE、cache、dependency 和验证对象。

Custom target 只编排 flash、debug、capture 或 report，不冒充 artifact。Workflow 必须依赖真实 target，
并保留退出码和工件路径。

## BSP 与 Source Ownership

Startup、vector、linker script、vendor/generated source、HAL、board driver 和相关 option 是项目/BSP 事实。
共享时使用单一 source manifest 或 CMake function，并明确：

- 哪个 target 拥有 startup/linker 和 generated input；
- 缺失 source 时 target 必须失败还是 feature 可省略；
- board/common object 是否重复编译或产生 duplicate symbol；
- feature define、source collection 与 vendor option 是否一致且不泄漏到 Host target。

整包链接和每 target 复制全量路径都不是稳定复用。

## Preset、Toolchain 与 Cache

Preset 固定 configure/build 参数和目录，toolchain 定义编译链接环境；IDE、CLI 与 CI 应消费同一 target，
不维护三份场景模型。切换 compiler、generator、linker script 或关键 ABI option 时必须判断 cache 是否
可复用，不能把 stale cache/module artifact 误诊为源码问题。

CMake 文件可按 source inventory、target policy、BSP binding 和 workflow helper 分层，但只在出现第二个
消费者后抽取共享部分。CMake 消费项目事实，不成为 Product/Profile/Capability 的第二事实源。

## 迁移与诊断

先盘点可构建 target，再建立一个代表性 artifact；仅提取已被复用的 source/policy，随后迁移 preset、
IDE 和脚本，最后删除兼容入口。目录移动、抽象重构和行为变化分开提交。

失败按阶段归因：configure（source/toolchain/input/option）、compile（include/module/definition）、
link（startup/linker/duplicate/overflow）、workflow（tool/port/path/permission）和 runtime（Host/QEMU/board
行为）。不能用同一条 `build failed` 覆盖这些故障域。
