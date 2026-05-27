# H747 Lab Dynamic Boundary Roadmap

本文收口 H747 当前三条动态边界实验线的角色、阶段顺序与近期验收重点：

- `app_lab`
- `posix_lab`
- `dev_loader`

目标不是同时推进三条主线，而是明确：

- 哪条是近期主线
- 哪条是兼容验证线
- 哪条是后置开发加速层

## 1. 当前主线选择

近期 H747 动态边界主线固定为：

```text
app_lab = resident App ABI mainline
```

它验证的主语是：

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

这条线的目标是证明：

- resident runtime/monitor 可以在 H747 上稳定加载并运行真实 App ELF。
- app 通过显式 capability table 获得能力。
- `lookup -> load -> abi -> argv -> start -> exit` 这条主链在 host 和 H747 上都可解释。

## 2. 另外两条线的角色

### 2.1 `posix_lab`

`posix_lab` 不是 `app_lab` 的上位路线，也不是动态 app 主模型。

它的角色固定为：

```text
POSIX / C ELF compatibility line
```

它继续负责：

- `spawn/load/wait` 兼容主链
- RAMFS / PATH / cwd / fd / pipe / stat 最小语义
- embedded ELF 样本与 `run-path` 兼容口径

它当前不负责：

- `CharmAppApi`
- `charm_app_main`
- resident App ABI 主叙事
- 反向定义 `app_lab` 的 capability table

### 2.2 `dev_loader`

`dev_loader` 不是产品 bootloader，也不替代 `app_lab`。

它的角色固定为：

```text
resident development acceleration line
```

它继续负责：

- resident receive / verify / launch-ready 原型
- RAM-backed session / storage / command runtime 验证
- console 与未来 USB transport 的薄前端

它当前不负责：

- 真实 app jump
- 取代 `app_lab` 的 resident runtime
- 作为 `app_lab` 第一里程碑前置条件

## 3. 近期阶段顺序

近期阶段顺序固定为：

```text
host smoke proof
  -> H747 build proof
  -> app_lab board smoke closure
  -> posix_lab compatibility follow-up
  -> dev_loader acceleration follow-up
```

换句话说：

- `app_lab` 先收 resident App ABI 主链。
- `posix_lab` 保持兼容验证，不抢主线。
- `dev_loader` 等 `app_lab` 稳定后再考虑接成开发时下载前端。

## 4. `app_lab` 第一里程碑

第一里程碑范围固定为：

- embedded App ELF
- `qspi:<name>` / `qspi:@<offset>:<size>` 只读 run-path
- generic file-backed path 的稳定 `not_supported` stub

第一里程碑不做：

- 真正的文件系统 backed App ELF
- USB receive / dev-loader handoff
- 新的 capability table 面
- 产品 bootloader / runtime framework 化

第一里程碑的官方样本组固定为：

- `hello_app`
- `player_min`

## 5. 验证梯度

### 5.1 Host-only 先行

近期主回归集：

- `app_abi_host_smoke`
- `app_abi_runtime_smoke`
- `app_abi_store_smoke`
- `dev_loader_session_smoke`
- `dev_loader_command_smoke`

### 5.2 H747 构建闭环

近期至少保持：

- `h747_lab_app_lab`
- `h747_lab_posix_lab`
- `h747_lab_dev_loader`

### 5.3 板上串口 smoke

板上优先顺序固定为：

1. `app_lab`
2. `posix_lab`
3. `dev_loader`

## 6. 与其它文档的关系

- `docs/h747_lab_spine_migration_boundary.md` 解释哪些 Spine/RTE 语义应迁到 H747。
- `docs/h747_lab_layering_contract.md` 解释 H747 项目层次与承载面。
- `docs/h747_lab_capability_contract.md` 解释 source-level capability/world 边界。
- `apps/app_lab/README.md` 解释 resident App ABI 主线。
- `apps/dev_loader/README.md` 解释 resident development-loader 原型。

本文只回答三条动态边界线“谁是主线、谁是副线、谁后做”，不替代各自的详细契约。
