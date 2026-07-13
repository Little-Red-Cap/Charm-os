# 构建模型早期取舍保留笔记

> status: `archived`
>
> scope: 复杂项目构建与目录迁移的可复用约束，不定义当前 CMake API

当前入口从 [`build route`](../../agent/routes/build.md) 进入；实际 target、preset、toolchain 和 source
ownership 必须检查当前 CMake。本文不保留 Player 场景或旧板卡路径。

## 显式 Artifact Target

不同固件、host app、QEMU image 或诊断场景若产生独立 artifact，应有独立 CMake target。用注释入口、
手改 source list 或单个 profile 变量反复改变同一 target，会让 IDE、缓存、依赖和验证对象含糊。

custom target 适合编排 flash、debug、capture 或报告，不应冒充 executable/library artifact。workflow
必须依赖真实 target，并保留其退出码和工件路径。

## BSP 与 Source Ownership

startup、linker script、vendor SDK、CubeMX/generated source、HAL、board driver 和 include/define 属于
项目/BSP 事实。共享时应有单一 source manifest 或 CMake function，而不是每个场景复制一份列表。

复用边界需要回答：

- 哪个 target 拥有 startup、vector table 和 linker options；
- generated source 缺失时哪些 target 必须失败，哪些 feature 可以不加入；
- board/common source 是否会被重复编译或产生 duplicate symbol；
- feature define 与实际 source collection 是否一致；
- vendor include/compile option 是否泄漏到无关 host target。

“把整个板包都链接进来”和“每个 target 复制全部路径”都不是稳定复用。

## Preset、Toolchain 与 Workflow

preset 固定 configure/build 参数和构建目录，toolchain 定义编译与链接环境；二者不拥有领域语义。
IDE、命令行和 CI 应消费同一组 target/preset，而不是维护三份场景模型。

构建目录必须与 preset/toolchain 对齐。切换 compiler、generator、linker script 或关键 ABI option 时，
需要显式判断缓存是否可复用；不能把 stale cache 或 module artifact 故障误诊为源码问题。

## CMake 分层

根或项目 `CMakeLists.txt` 负责入口和 target 注册；可复用 CMake 文件可分别承载 source inventory、
target policy、toolchain/BSP 绑定和 workflow helpers。是否拆文件由重复消费者和变更边界决定，不按
“每种名词一个 package”机械扩展。

CMake 不应成为第二份 Product/Profile/Capability 真相。它消费明确项目事实来创建 target，不反向
定义应用或 Core 语义。

## 迁移顺序

1. 盘点当前可构建 target 的真实 source、definition、link option 和生成前置条件。
2. 为一个代表性 artifact 建立独立 target，并验证 configure/build/map/run 入口。
3. 仅将已被第二个 target 复用的部分提取为共享 source/policy function。
4. 让 preset、IDE 与脚本引用这些 target，再迁移其它场景。
5. 最后删除旧 source list、兼容变量和重复 workflow。

目录移动与 CMake 抽象不要在同一提交中同时改变语义。先保存可回退边界，再修 stale path 和 ownership。

## 诊断分类

- configure 阶段：缺失 source、toolchain、generated input 或互斥 option；
- compile 阶段：include/module/definition 漂移；
- link 阶段：startup/linker/重复对象/region overflow；
- workflow 阶段：flash/debug tool、port、artifact 路径或权限；
- runtime 阶段：构建成功但 board/QEMU/host 行为失败。

不同阶段不能用同一条“build failed”结论覆盖。旧提案的 Package 命名和迁移完成度不构成当前事实。
