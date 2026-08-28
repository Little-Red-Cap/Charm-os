# Build Route

## 文档状态

- `status`: `supporting`
- `scope`: CMake、preset、toolchain 与 target build 路由

## 最短路径

1. [`CMakePresets.json`](../../../CMakePresets.json)
2. [Project 文档入口](../../project/README.md)
3. 目标目录的 README、CMake 与 source
4. [CMake skill](../skills/charm-cmake/SKILL.md)

构建事实以 preset、CMake、target 和真实 consumer 为准；行为或使用入口变化时同步对应 README。
OnlyCore 的公共构建入口只有 `Charm::core`。
