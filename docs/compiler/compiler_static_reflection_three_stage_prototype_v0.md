# Static Reflection Compile Probe v0

## 文档状态

- `status`: `exploration`
- `scope`: hosted reflection 与 freestanding residue 的编译边界
- `authority`: [`README.md`](README.md)

当前探针只验证：

```text
hosted TU can compile with <meta>
freestanding TU can compile a plain residue header
```

它不实现 reflection extractor，也不证明两者之间存在自动生成链。

## 入口

- [`compiler_static_reflection_three_stage_probe.ps1`](../../scripts/compiler_static_reflection_three_stage_probe.ps1)

脚本使用指定 `arm-none-eabi-g++`：

1. 生成并编译一个 `-std=c++26 -freflection` hosted fixture；
2. 写入一个手工定义的 plain C++ header；
3. 用 `-ffreestanding -fno-exceptions -fno-rtti` 编译 consumer fixture；
4. 检查两个 object 文件存在。

## 已证明

- 当前工具链可编译最小 `<meta>` fixture；
- freestanding consumer 不需要 include `<meta>`；
- plain enum、struct 和 constexpr table 可作为 residue 形状。

## 未证明

- hosted fixture 会读取项目 source facts；
- residue 由 reflection 输出生成；
- field identity、source provenance 或 semantic equivalence 被保持；
- World IR、freeze、lowering manifest 或 codegen pipeline 存在；
- 普通 firmware target 可以全局启用 `-freflection`。

## 约束

- `<meta>` 只允许出现在 hosted 实验 TU。
- Firmware/runtime modules 不依赖 hosted reflection headers。
- 任何未来 extractor 必须增加 source fixture、生成步骤、内容断言和负例，不能复用当前
  “两个独立编译通过”作为端到端证据。

## 运行

```powershell
./scripts/compiler_static_reflection_three_stage_probe.ps1 -Clean
```

默认 ARM 工具链路径是本机约定，可用 `-ArmGpp` 覆盖。输出位于
`out/compiler-static-reflection-three-stage-probe`。
