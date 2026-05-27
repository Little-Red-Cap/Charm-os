# static_reflection_probe

这个示例只做一件事：确认最新 `arm-none-eabi-g++` 已经能在 Charm 的构建链里使用
C++ static reflection。

它是编译期 smoke，不产出可运行程序。当前检查项包括：

- `__cpp_impl_reflection`
- `<meta>`
- `^^T`
- `[:R:]`
- `std::meta::is_type`
- `std::meta::identifier_of`
- `std::meta::type_of`
- `std::meta::constant_of`
- `std::meta::nonstatic_data_members_of` 可以发现 spec 字段形状
- reflection value 可以作为模板参数描述 `Requirement/Provided`
- `ComponentDesc` 的 requirements 可以由多个 provider component 在编译期满足

当前样例刻意贴近 RTE/capability composition 语义：用
`Requirement<^^cap::TextSink, ^^role::log>` 和
`Provided<^^cap::TextSink, ^^role::log>` 表达 kind/role，再在编译期确认匹配成功、
缺失能力失败，以及 app requirements 被多个 provider component 满足。

它仍然是局部 proof，不提升为公共模块。

当前 probe 只用成员反射检查字段数量与名字。不要在这里把
`nonstatic_data_members_of()` 返回的 `vector<info>` 直接作为类型 reify 输入；这版工具链在该路径上仍有限制。

构建与运行：

```powershell
cmake -S Examples/system/static_reflection_probe -B Examples/system/static_reflection_probe/cmake-build-static-reflection-probe -G Ninja -DSTATIC_REFLECTION_GXX="D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest/bin/arm-none-eabi-g++.exe"
ctest --test-dir Examples/system/static_reflection_probe/cmake-build-static-reflection-probe --output-on-failure
```

如果最新编译器已经在 `PATH` 里，也可以不传 `STATIC_REFLECTION_GXX`。如果 `PATH`
还指向旧的 15.2，请显式传新路径。
