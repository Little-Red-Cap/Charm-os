# CMake 示例模板入口

本目录收纳示例工程共用的 CMake 模板与辅助约定，本身不是独立运行示例。

当前建议先看：

- [`ExampleTemplate.cmake`](ExampleTemplate.cmake)

## 这个模板提供什么

- `charm_example_init(...)`
- `charm_example_sources(...)`
- `charm_example_sources_filtered(...)`
- `charm_example_link_charm(...)`
- `charm_example_sdl3_options()`
- `charm_example_link_sdl3(...)`

## 使用提醒

- 新示例优先复用这里的模板，避免每个目录各写一套 Charm 接入方式。
- 如果你想看实际使用方式，可回到 [`../ui/README.md`](../ui/README.md) 中的 Vivid/SDL3 示例。
