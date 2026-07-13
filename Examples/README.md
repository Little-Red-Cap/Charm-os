# Examples 入口

Examples 用于验证局部接口、运行路径或板级装配。示例存在只证明对应 fixture 存在，不代表跨平台
能力、产品成熟度或当前测试结果。

## 按主题进入

| 主题 | 入口 |
|---|---|
| Kernel、RTOS、QEMU、POSIX | [`kernel/README.md`](kernel/README.md)、[`posix/README.md`](posix/README.md) |
| Init / materialize / observe | [`init/README.md`](init/README.md) |
| IO、FS、USB | [`io/README.md`](io/README.md)、[`fs/README.md`](fs/README.md)、[`usb/README.md`](usb/README.md) |
| System 与 service | [`system/README.md`](system/README.md)、[`service/README.md`](service/README.md) |
| App ABI、resident loader | [`app_abi/README.md`](app_abi/README.md)、[`dev_loader/README.md`](dev_loader/README.md) |
| Audio、UI、Ink | [`audio/README.md`](audio/README.md)、[`ui/README.md`](ui/README.md)、[`ink/README.md`](ink/README.md) |
| HAL、boot、algorithm | [`hal/README.md`](hal/README.md)、[`boot/README.md`](boot/README.md)、[`alg/README.md`](alg/README.md) |
| Shell / ModuleX | [`shell/README.md`](shell/README.md) |
| 项目工程 | [`project/README.md`](project/README.md) |
| 共用 host 辅助代码 | [`common/README.md`](common/README.md) |
| CMake 接线 | [`cmake/README.md`](cmake/README.md) |

## 使用规则

- 先读对应目录 README，再运行该目录声明的 target、runner 或 smoke。
- 本地构建目录使用仓库约定的 `cmake-build-*` 命名，不在示例树内创建 `build/`。
- SDL、toolchain、board 和外设依赖由具体示例声明，不在总入口维护安装步骤。
- 当 README 与源码/CMake 冲突时，以 target、source collection 和当次运行结果为准并修正文档。

仓库总入口见 [根 README](../README.md)，构建约定见
[`build route`](../docs/agent/routes/build.md)。
