# Vivid UI 回放/稳定性最小闭环

目标：用 **dump/replay + CI 指标** 实现可复现的 UI 问题定位与回归验证。

## 1. 推荐脚本

位置：`Examples/ui/vivid/soa_demo/soa_ci.ps1`

### CI 路径（推荐）

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode ci
```

产物：

- `artifacts/soa_ci/soa_ci.vcmd`（命令回放文件）
- `artifacts/soa_ci/soa_ci.log`（完整日志）

### 只做 dump（本地复现最小包）

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode dump
```

### 回放（验证复现）

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode replay -ReplayPath artifacts/soa_ci/soa_ci.vcmd
```

可选 `-Tile` 切换为 tile backend 回放：

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode replay -ReplayPath artifacts/soa_ci/soa_ci.vcmd -Tile
```

## 2. 直接命令（无需脚本）

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --soa-ci --regress-ui --dump-cmd=soa_ci.vcmd
```

回放：

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --replay-cmd=soa_ci.vcmd --backend=full
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --replay-cmd=soa_ci.vcmd --backend=tile
```

## 3. 最小复现包建议

提交或传递以下三项即可复现 UI：

1) `*.vcmd`  
2) 运行日志（含 `[soa-ci]` 输出）  
3) 对应 commit hash

## 4. 常用排障点

- `ok=0`：优先看 `reason=` 与 `failed_cmds`  
- `img_new_after_lock != 0`：说明 lock 后仍有图片注册  
- `cmd_budget`：命令数超预算  
- `replay_*` hash 不一致：回放不一致  
