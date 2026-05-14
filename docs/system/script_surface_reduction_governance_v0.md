# Script Surface Reduction / Evidence Harness Governance v0

## 定位

这份文档是 `scripts/` 的治理合同，不是新的脚本实现计划。

目标是把脚本重新压回 evidence harness：

```text
source / schema / contract / shared library
  own system semantics

scripts/
  orchestrate, collect, validate, package, report
```

脚本可以证明一次运行、打包一次证据、校验一个 artifact、生成一份 report；脚本不应该成为第二套 system compiler、opening judgment、compare 或 runtime 语义实现。

本刀只立法和盘点，不删除脚本、不移动文件、不改 CI、不重构实现。

## 四层脚本分层

### `entrypoint`

CI 或人工入口。

职责：

- 接收参数。
- 串接已有 harness / adapter / library。
- 选择输出目录。
- 返回明确 exit code。

边界：

- 只编排，不承载系统语义。
- 不复制 exporter、validator、compare 的核心判断。
- PowerShell 更适合留在这一层。

### `harness`

运行环境与证据采集层。

职责：

- 运行 build / test / QEMU / host smoke。
- 采集日志、summary、report、check。
- 保证证据可复现。

边界：

- 可以知道如何运行某个环境。
- 不应该重新定义 artifact 的语义字段。
- 不应该把串口日志或 console 输出反向变成系统模型。

### `adapter`

已有 summary / schema / contract 到 artifact 的投影层。

职责：

- 从已定义输入读事实。
- 输出已定义 schema/kind。
- 做最小格式转换和路径归一。

边界：

- 不发明新的 compare brain。
- 不把 selection / route / opening judgment 只写在脚本里。
- 如果 adapter 变成主要语义载体，应回迁到 contract、schema、shared library 或源码侧。

### `library`

共享逻辑层。

职责：

- 承载跨 exporter / validator / compare / report 复用的纯逻辑。
- 提供可测试、可导入、可组合的函数。
- 降低同族脚本的 5 到 7 件套复制。

边界：

- 优先 Python library，而不是 PowerShell 巨型函数堆。
- 复杂数据判断应进入 library 或源码侧。
- library 也不得绕过 schema / contract 自行定义产品语义。

## 硬规则

### 新 artifact 不默认长出脚本家族

新增 artifact 时，不再默认新增：

```text
exporter + validator + smoke + inspect + report + compare + workspace
```

必须先判断是否能复用既有 exporter / validator / report / check 框架。只有当现有框架无法表达 contract 中已经定义的行为时，才新增脚本。

### 语义不得只存在于脚本

以下语义不得只写在 Python / PowerShell 里：

- compare verdict。
- selection policy。
- opening judgment。
- route / explain / handoff 判决。
- runtime session verdict。
- failure / drift / collapse 的正式含义。

这些语义必须能在源码、schema、contract 或 shared library 中找到第一解释位置。脚本可以调用和投影，不应成为唯一解释者。

### 行数阈值

脚本收敛候选规则：

- `>= 500` 行：进入收敛候选清单。
- `>= 1000` 行：必须有拆分计划、冻结说明或迁移理由。
- `>= 5000` 行：默认视为结构性风险，后续应优先拆分或归档。

阈值不是为了追求短文件，而是防止脚本承担过多职责。

### PowerShell 角色收窄

PowerShell 优先作为：

- CI / 人工入口。
- 平台命令编排。
- Windows 路径与进程 glue。

PowerShell 不应继续增长为：

- 长篇 JSON 语义处理器。
- compare engine。
- 大型 report renderer。
- 多对象 workspace brain。

复杂数据逻辑优先迁移到 Python library、schema-aware validator，或更靠近源码的实现层。

## 当前盘点基线

详细族群清单与第一批 pilot 候选见：

- `docs/system/script_surface_reduction_inventory_v0.md`

盘点口径：

- 当前 Git 已跟踪的 `scripts/` 下 `.ps1` 与 `.py` 文件。
- 不把当前隔离的未跟踪 `world_shelf_review` 文件计入治理基线。
- 使用 PowerShell 只读统计命令，未修改仓库文件。

当前结果：

| 指标 | 数值 |
| --- | --- |
| 脚本文件总数 | 255 |
| PowerShell 文件 | 175 |
| Python 文件 | 80 |
| 脚本总行数 | 100785 |
| `>= 500` 行脚本 | 54 |
| `>= 1000` 行脚本 | 9 |
| 最大脚本 | `scripts/inspect_system_compiler_artifact_report.ps1` |
| 最大脚本行数 | 10063 |
| system-compiler/front-page/witness/biography/world 相关脚本 | 194 |
| system-compiler/front-page/witness/biography/world 相关行数 | 68474 |

这说明当前膨胀主体不是 ARMv7-A QEMU lower-half smoke，而是 system compiler / front page / opening flow / witness / biography / world compare 的证据与解释流水线。

## 后续收敛优先级

第一批收敛候选优先从 system-compiler / front-page / opening-flow 族群中选择。

推荐顺序：

1. 盘点同族 `export / validate / inspect / report / compare / smoke / workspace` 的重复结构。
2. 抽出 Python shared library，先减少复制，不改 artifact 语义。
3. 把只存在于脚本中的 selection / compare / route 口径回指到 contract。
4. 冻结或归档超过 1000 行且不再适合继续增长的脚本。
5. 最后再考虑合并入口或迁移 CI。

ARMv7-A QEMU lower-half smoke 暂不作为第一批重构对象。它们承担真实机器证据采集，优先保持稳定。

## 非目标

本 contract v0 不做：

- 不删除任何脚本。
- 不移动脚本目录。
- 不修改 CI workflow。
- 不新增 schema、validator、smoke 或 compare brain。
- 不把治理合同变成新的脚本框架实现。
- 不把历史阶段材料重新抬回默认文档入口。

## 验收标准

本刀完成后应满足：

- `docs/README.md` 与 `docs/system/README.md` 能找到这份治理合同。
- `docs/architecture/system_compiler_vocabulary_v0.md` 有 `EvidenceHarness` / `ScriptSurface` 最小词条。
- `git diff --check` 通过。
- 当前未跟踪 `world_shelf_review` 文件仍保持隔离。
- 没有新增 schema、script、validator、smoke、compare verdict。
