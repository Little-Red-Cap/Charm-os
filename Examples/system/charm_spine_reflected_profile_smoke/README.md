# Reflected Profile Smoke

## 文档状态

- `status`: `exploration`
- `scope`: Host-only reflection 与 compile-time binding rejection experiment
- `source`: [`main.cpp`](main.cpp) 与 [`CMakeLists.txt`](CMakeLists.txt)

所有 reflected spec、profile、projection 和 report type 都留在 fixture 内。本 target 不是 public module、
Charm Core model、manifest、generator、service locator 或 runtime framework。

## 试验边界

```text
local reflected spec
-> compile-time binding checks
-> accepted or blocked
-> local init/context/evidence projection
```

fixture 验证缺失、重复、额外、陈旧和错误 role binding 会被拒绝；只有 accepted result 可以进入本地 init
graph。blocked result 不选择 fallback provider，也不生成 provider evidence。evidence 是 init control flow
之外的只读投影。

status、report section、builder、capacity 和 reflected type 都是测试实现，不能据此建立 Charm
`Profile/Provider/ContextView` 或 evidence ABI。Host compiler 通过也不证明 QEMU、H747 或其它 toolchain。

## 验证

在调用方拥有的同一个 build directory 中配置、构建和运行本 target，不为重复验证创建并行 build tree。
compiler flag、target、assertion 与 pass result 以 CMake/source 和当次执行为准。
