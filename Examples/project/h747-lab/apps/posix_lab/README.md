# H747 POSIX Lab

## 文档状态

- `status`: `supporting`
- `scope`: H747 上的 C/POSIX ELF compatibility shell
- `source`: [`posix_lab.cpp`](posix_lab.cpp)、[`app.cmake`](app.cmake)

`posix_lab` 使用 POSIX `main(argc, argv, envp)`，与 `app_lab/dev_loader` 的
`charm_app_main(CharmAppApi, ...)` 分离：

```text
POSIX ProgramImage -> spawn -> load_image -> start -> waitpid
```

## Monitor

| 命令 | 作用 |
|---|---|
| `elf list` / `elf status` | builtin fixture 与 runtime/load/cwd/PATH diagnostics |
| `elf run <name> [args...]` | 运行 embedded fixture |
| `elf run-path <path> [args...]` | generic path surface |
| `elf smoke` | 运行 source-defined compatibility subset |

fixture name、smoke membership、参数和输出由 `posix_lab.cpp` 维护。

## 边界

本 target 验证 C/POSIX ELF load/entry、spawn/start/wait result、argv/env/cwd/PATH、RAMFS file、fd、
pipe、stat 和 terminal fixture。它不拥有 resident download/Store、`CharmAppApi`、Player policy、package
manager 或通用 process runtime。

Embedded ELF 是已接线 source。file-backed executable backend 尚未接入，因此 `elf run-path <path>`
稳定返回 `not_supported`，不得 fallback 到无关 embedded fixture。load region/cache 属于 H747 project；
build-only 不证明 POSIX runtime 成功。

## 验证

从 `Examples/project/h747-lab` 复用 `build-h747-lab-posix-lab-debug`。板级验收至少执行 `elf run hello`、
`elf smoke`、unsupported `elf run-path` 与 `elf status`；结果以当次 console log 为准。

动态入口分工见
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md)。
