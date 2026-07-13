# Vivid Evidence Lab Manifest v0

## 文档状态

- `status`: `supporting`
- `scope`: CTest-gated Vivid evidence fixture registry 与漂移检查
- `authority`: [`evidence_lab_manifest_demo`](../../Examples/ui/vivid/evidence_lab_manifest_demo/)、
  [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md)、各 demo `CMakeLists.txt`

本文不复制 registry 行、case 总数、stdout final line 或当前 anchor 清单。它只定义 manifest 的角色和
一致性规则。

## Registry 边界

manifest 将 fixture 连接到可验证入口：

```text
demo -> stdout/CTest gate -> evidence axes -> primary law
```

每个 row 包含：

| field | contract |
|---|---|
| `run` | stable demo identity，与 stdout/CMake gate 一致 |
| `tag` | stable short stdout domain |
| `cases` | 该 demo final gate 的显式 case 数 |
| `axes` | stdout 或 primary law 实际覆盖的 evidence domain |
| `primary_doc` | 解释 evidence meaning 且指回 demo path 的唯一首选文档 |

run/tag 必须唯一，cases 必须为正，primary doc 必须存在。多个 demo 可以共享同一 primary law，但每个
demo path 都必须在该 law 中可发现。

## Coverage 法律

- axis 只表示 fixture 已输出或 paired law 已定义的证据，不表示产品能力完成；
- `AxisCausal` 必须满足 [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)，不能只打印
  `causal_chain=1`；
- vocabulary fixture 只验证字段/helper verdict，不证明 runtime behavior；
- manifest fixture 只验证 registry、route 和 gate 同步，不运行全部 fixture；
- Host fixture、screenshot/hash 和 CTest pass 都不能替代产品或真实板证据。

## Conformance

`Examples/ui/vivid/evidence_lab_manifest_demo` 检查：

- registry shape、run/tag uniqueness 和 required axis coverage；
- stdout law 与 demo CMake pass gate 一致；
- primary doc 指回 demo，并为 causal row 提供 causal evidence；
- demo-side helper、law vocabulary 与 runtime-native ledger 的 promotion boundary 未混淆。

快速入口：

```powershell
./scripts/vivid_evidence_lab_manifest_smoke.ps1
```

该脚本会构建独立 fixture；磁盘受限环境应显式复用已批准的 build root，不能无意创建平行构建树。

## 维护

新增、删除、拆分或重命名 gated demo 时同步：

1. `Examples/ui/vivid/evidence_lab_manifest_demo/main.cpp` registry；
2. `vivid_evidence_stdout_law.md` final gate；
3. demo `CMakeLists.txt` pass/fail gate；
4. primary law 的 demo route；
5. 推荐入口变化时的 [`README.md`](README.md)。

具体 fixture 行和 token 由上述事实源维护，本文不建立第二份快照。
