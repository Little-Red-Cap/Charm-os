# ModuleX 格式草案（最小可运行版）

本草案用于描述当前模块装载的最小格式，以便后续扩展到真正的 XIP/动态加载。

## 头部（ImageHeader）
- `magic`/`version`: 文件标识与版本
- `entry_offset`: 入口相对 text 段起始的偏移
- `text_offset/ro_offset/data_offset/rel_offset/sym_offset/str_offset/dep_offset`
  - 显式段偏移（相对镜像起始）
  - 方便 XIP/随机布局
- `text_size/ro_size/data_size/bss_size/rel_size/sym_size/str_size/dep_size`
  - 段大小

## 重定位（Reloc）
```
offset: 需要修补的目标地址（相对镜像起始）
type:   none / abs_addr / rel32
sym_index: 符号索引（0xFFFFFFFF 表示使用镜像基址）
addend: 追加偏移
```

语义：
- `abs_addr`: *(target) = sym_addr + addend
- `rel32`: *(target) = (sym_addr + addend) - (target + 4)

其中 `sym_addr`：
- external 符号：由 resolver 返回的绝对地址
- 非 external：`text_base + symbol.value`

## 符号表（Symbol）
```
name_offset: 字符串表偏移
value:       符号值（相对 text 起始）
kind:        local / global / external
```

字符串表（strtab）：
- 由 `str_offset/str_size` 指定
- `name_offset` 指向该区域内的 C 字符串

依赖表（Dependency）：
- `dep_offset/dep_size` 指定依赖数组
- 每项包含 `name_offset` 与 `version_offset`
- 两个 offset 均指向 strtab 内的字符串

版本匹配（最小版）：
- 精确匹配：`v1` == `v1`
- `^1`：主版本匹配（`v1.*`）
- `~1`：主版本匹配（与 `^1` 相同，后续可扩展为次版本约束）

依赖校验结果（DepStatus）：
- `ok`：是否全部依赖满足
- `failed_index`：第一个失败的依赖索引
- `error`：
  - `ok`
  - `bad_name`（name_offset 越界/字符串表异常）
  - `resolve_failed`（找不到模块或版本不匹配）

## 当前 demo 验证点
- `abs_addr`：写入 entry 地址
- `rel32`：写入 PC-relative 偏移
- `external`：通过 resolver 绑定宿主符号地址

## 下一步扩展方向
- 支持更多重定位类型（如 GOT/PLT）
- 引入版本化符号与依赖图
- 支持多段与只读映射策略
