# ProgramImage / ELF 最小加载器草案（v0）

目标：在不引入完整动态链接器的前提下，定义一个可接入 ProgramImage 的最小 ELF 加载接口与执行链路，为后续扩展保留边界。

## 1. 目标与非目标

目标：
- 定义 ELF v0 的加载边界与 `ProgramImage` 对接方式。
- 明确入口 ABI 与 argv/envp 交付责任分界。
- 保持 `spawn -> load_image -> start_image` 链路不变。

非目标（v0 不做）：
- 动态链接器/共享库（PT_INTERP / DT_NEEDED）。
- 完整权限模型/ASLR/COW。
- 全量 ELF 规范覆盖。

## 2. 支持范围（v0）

- 仅支持静态、扁平的 ELF 映像（无动态段）。
- 只解析必需的 Program Header（PT_LOAD）与入口点。
- 可选：最小符号表解析（用于调试或入口验证）。

## 3. 入口 ABI

入口与 ProgramImage 统一：

```
int entry(int argc, char** argv, char** envp);
```

- argv/envp 的构建仍由 `start_image` 负责。
- ELF loader 只负责映像装载、重定位（如需）与入口地址确定。

## 4. 接口草案

```cpp
struct ElfLoadConfig {
    const void* image_base;  // 指向 ELF 文件数据
    std::size_t image_size;
    void* load_base;         // 目标加载地址（v0 可选固定）
};

util::Result<ProgramImage> load_elf_image(const ElfLoadConfig& cfg) noexcept;
```

## 5. 加载步骤（v0）

1. 校验 ELF header（magic / class / machine / entry）。
2. 解析 Program Header，只处理 PT_LOAD。
3. 把可加载段映射到 `load_base`（或原地）。
4. 计算入口地址并生成 `ProgramImage`。

## 6. 迁移路径

v0：
- 只支持静态 ELF，入口直接 `e_entry`。
- 不做动态段、重定位复杂语义。

v1：
- 最小重定位支持（只覆盖常见 relocation）。
- 保留 `ProgramImage` 入口 ABI 不变。

v2：
- 动态链接器/共享库（如确有必要）。

## 7. 验收样本

最小目标程序：
- `hello`
- `argv_dump`
- `exit_code`

验收标准：
- ELF 入口可运行并正确返回 exit code。
- argv/envp 传递正确。
