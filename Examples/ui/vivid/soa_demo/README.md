# Vivid SoA CI / Replay

## 文档状态

- `status`: `supporting`
- `scope`: `soa_ci.ps1` 的 ci、dump、replay workflow
- `authority`: [`soa_ci.ps1`](soa_ci.ps1)、[`CMakeLists.txt`](CMakeLists.txt)、`main.cpp`

该 fixture 验证 SoA/table/tree/record-replay 的内部行为，不进入 Evidence Lab manifest，也不替代产品或
真实 backend 证据。

## 模式

```powershell
./Examples/ui/vivid/soa_demo/soa_ci.ps1 -Mode ci
./Examples/ui/vivid/soa_demo/soa_ci.ps1 -Mode dump
./Examples/ui/vivid/soa_demo/soa_ci.ps1 -Mode replay -ReplayPath artifacts/soa_ci/soa_dump.vcmd
./Examples/ui/vivid/soa_demo/soa_ci.ps1 -Mode replay -ReplayPath artifacts/soa_ci/soa_dump.vcmd -Tile
```

`ci` 运行 CTest；`dump` 生成 command replay；`replay` 用 full 或 tile backend 消费已有 replay。font
provider/fallback/UTF-8 replacement 的附加要求由脚本 switch 定义，不在本文复制。

## Build 与产物

脚本默认会配置并构建本目录下的 `cmake-build-soa-ci`，该目录可能超过 1 GiB。磁盘受限或并行工作区必须
显式传入已批准的 `-BuildDir`，避免创建多个平行 build tree；运行后按工作区策略清理生成物。`-NoBuild`
只跳过 build step，不跳过 configure。

`-OutDir` 默认写入 `artifacts/soa_ci`。最小复现证据包括：

1. `*.vcmd` replay；
2. `soa_ci.log`；
3. 对应 commit hash 与 full/tile mode。

失败先看 process exit code、CTest failure、`reason`、failed command、overflow/budget 和 replay hash。具体 token
属于当前 source/CTest，不在 README 建立第二份清单。
