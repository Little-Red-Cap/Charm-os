# H747 App Lab

## 文档状态

- `status`: `supporting`
- `scope`: embedded App ELF 与 QSPI Store v1 的 H747 baseline
- `source`: [`app_lab.cpp`](app_lab.cpp)、[`app.cmake`](app.cmake)

`app_lab` 内嵌已知 App ELF/Store fixture，用于在不依赖 packetstream download 的情况下验证 loader、
ABI、argv、capability 与 QSPI。它不拥有 resident download/install policy，也不使用 POSIX `main`。

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

## Monitor

| 范围 | 命令 |
|---|---|
| embedded App | `app list`、`app run <name> [args...]` |
| QSPI Store | `app store status|list|install`、`app run-path qspi:<name>` |
| raw QSPI range | `app run-path qspi:@<offset>:<size>` |
| diagnostics / regression | `app status`、`app smoke` |
| file-backed path | `app run-path <other>`，稳定返回 `not_supported` |

命令参数、fixture、Store field 和 status token 由源码与 capture script 维护。

## Fixture 与 Runtime

- `hello_app` 验证 C ABI entry、console、argv 与 exit recovery。
- `player_min` 验证 diagnostic display/input/time capability；它不是产品 UI 或 full-frame policy。
- Embedded 与 QSPI 汇入 `AppImage -> staged source -> ELF loader -> AppRuntime -> CharmAppApi`。
- QSPI lookup/stage 复用 [`Examples/app_abi`](../../../../app_abi/README.md)；H747 只提供 media、D1 execute
  region、cache prepare 与 capability binding。
- ModuleX 仍是同一 App ABI 的另一 image format；`app_lab` 不定义第二入口模型。

固定 ELF load base 为 `0x24070000`，必须与 image link address 一致。Generic filesystem executable
source 尚未接入。

## 验证

Build、flash、capture、manual acceptance 与 retained board evidence 由
[`h747_lab_app_lab_smoke.md`](../../docs/h747_lab_app_lab_smoke.md) 统一维护。动态入口分工见
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md)。
