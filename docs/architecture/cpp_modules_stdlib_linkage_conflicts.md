# C++ 模块下的 stdlib linkage 冲突排查与解决

适用错误形态：
- `conflicting language linkage for imported declaration 'std::__is_constant_evaluated()'`
- 报错链路常见为 `<concepts>/<version>/<c++config.h>` 之类的标准库内部头

## 现象特征
- 只在 **模块编译** 时出现（非模块 TU 不触发或更少触发）
- 报错常指向 `std::__is_constant_evaluated()` / `__glibcxx_assert` 等内部符号
- 在 ARM GNU Toolchain + HAL/USB 头混用时更容易出现

## 根因归类（高概率）
1) **C 头污染了链接语言**
   - 某个 C/C++ 头的 `extern "C"` 没有正确闭合
   - 或把 `__cplusplus` 状态污染到了标准库头的解析阶段
2) **同一头以不同语言模式被导入**
   - 部分 TU/模块被当成 C 编译，部分当成 C++ 编译
   - 导致同名符号出现 C 与 C++ linkage 的冲突
3) **在模块接口里直接包含 C 头**
   - `module;` 全局片段里夹入了不干净的 C 头
   - 标准库头被间接包裹进 `extern "C"`

## 排查步骤（建议顺序）
1) **锁定触发头**
   - 从报错栈顶往回看，找到首次出现的标准库头（如 `<concepts>`）
2) **检查最近引入的 C/C++ 头**
   - 搜 `extern "C"` 是否成对
   - 搜是否存在 `#define __cplusplus` 或宏污染（极少见）
3) **检查模块接口**
   - `module;` 片段里尽量避免直接包含 C 头
   - 把 C 头收口到 `.cpp` 或者本地 shim 头

## 快速绕过（不伤架构）
当模块里只是用到 `std::same_as`、`std::invocable` 这类概念时：
- **优先替换为 `std::is_same_v` + 自定义 concept**
  - 避开 `<concepts>` 引入
  - 例：`SameAs = std::is_same_v<A,B> && std::is_same_v<B,A>`

## 推荐修复方案（长期）
1) **清理 C 头的边界**
   - 所有 C 头必须严格包裹 `extern "C"`，且成对闭合
2) **模块接口内不直接包含 C 头**
   - 通过 shim 或 implementation 单元隔离
3) **编译模式一致**
   - 保证同一头不会被 C/C++ 混合编译
4) **减少 `<concepts>` 依赖**
   - 能用 type traits 的地方尽量不用 `<concepts>`

## 处理策略准则
- **先绕过、再根治**：保证构建稳定后再做清理
- **模块接口要“干净”**：避免把 C 头直接暴露给导入方

## 本仓库已验证有效的处理
- 在 `fs_stream.cppm` 中移除 `<concepts>`，改用 `std::is_same_v` + 本地 `SameAs`，即刻消除冲突

## 头文件污染检查清单（快速）
1) `extern "C"` 是否成对闭合（尤其是 HAL/USB/CMSIS 头）
2) 是否有 `#ifdef __cplusplus` 开始却缺少收尾
3) 是否在 `module;` 片段直接包含 C 头
4) 是否有 `#define __cplusplus` / `#undef __cplusplus` 之类宏污染
5) 是否存在同一头被 C/C++ 混合编译的情况（构建系统排查）
6) 是否在标准库头之前引入了异常的编译开关（例如禁用异常/RTTI 的不对称切换）
7) 是否有第三方头在 `extern "C"` 内部继续 `#include <concepts>` / `<version>` 等
