# C++ Module stdlib linkage 冲突记录

## 文档状态

- `status`: `supporting`
- `scope`: `conflicting language linkage for imported declaration` 的一次工具链事件
- `source`: [`fs_stream.cppm`](../../Modules/io/fs/fs_stream.cppm)

本文不是 C++ modules 通用契约。仓库只确认过一个局部 workaround，未证明统一根因。

## 已确认事实

`fs_stream.cppm` 当前使用 `<type_traits>` 与本地双向 `SameAs` concept：

```cpp
template <class A, class B>
concept SameAs = std::is_same_v<A, B> && std::is_same_v<B, A>;
```

该改动曾绕开 `std::__is_constant_evaluated()` 的 language linkage 冲突。仓库其它模块仍正常使用
`<concepts>` 与 `std::same_as`，因此不能把“移除 `<concepts>`”提升为全局规则。

## 排查顺序

1. 保留完整编译命令、toolchain 版本、module interface 和 include stack。
2. 检查 `extern "C"` 是否闭合，以及 C/HAL/CMSIS 头是否在 global module fragment 中包入标准库头。
3. 检查同一文件是否被 C/C++ 或不同 module flags 编译，并核对 BMI/GCM 是否来自同一配置。
4. 用最小复现分别验证：移除可疑 C 头、隔离到 implementation TU、替换单个 `<concepts>` 用法。

只有最小复现确认后，才能把原因归为头文件 linkage 污染、编译模式不一致或工具链 modules 缺陷。
局部 `is_same_v` workaround 可以用于解除阻塞，但不能替代根因定位，也不应推动全仓移除 concepts。
