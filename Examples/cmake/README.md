# CMake 示例模板入口

[`ExampleTemplate.cmake`](ExampleTemplate.cmake) 提供示例工程共享的 CMake helper，本目录不是可运行
示例。当前入口包括：

- `charm_example_init/sources/sources_filtered`；
- `charm_example_link_charm`；
- `charm_example_sdl3_options/link_sdl3`。

新示例应复用这些 helper；实际参数和行为以模板源码及其 CMake consumer 为准。
