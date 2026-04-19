# Build Route

## 适用场景

- CMake / preset / 构建接线
- 示例构建入口
- 构建目录、配置与 toolchain 相关问题

## 最短阅读顺序

1. [`../../../CMakePresets.json`](../../../CMakePresets.json)
2. [`../../project/README.md`](../../project/README.md)
3. [`../skills/charm-cmake/SKILL.md`](../skills/charm-cmake/SKILL.md)
4. 目标目录下的 `README.md` 或示例入口

## 先不要做什么

- 不要无理由绕开 preset。
- 不要发明与仓库约定不一致的构建目录命名。
- 不要只改构建脚本，不同步相关入口文档。

## 完成前自检

- preset / toolchain / 构建目录是否一致。
- 新入口是否可从 README 找到。
- 构建或使用方式变化是否已回写文档。
