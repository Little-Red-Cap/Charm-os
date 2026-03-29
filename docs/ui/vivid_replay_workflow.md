# Vivid UI 回放与稳定性最小闭环

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

## 2. 直接命令（无脚本）

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --soa-ci --regress-ui --dump-cmd=soa_ci.vcmd
```

回放：

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --replay-cmd=soa_ci.vcmd --backend=full
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --replay-cmd=soa_ci.vcmd --backend=tile
```

## 2.1 单色/灰度后端调试

BW1（阈值可调，强制走 tile 路径）：

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --bw1 --bw1-threshold=140 --soa-tile
```

2bit 灰度（可调抖动强度，强制走 tile 路径）：

```powershell
g:\Project\Codex\Charm-os\Examples\ui\vivid\soa_demo\cmake-build-debug\Debug\vivid-soa-demo.exe --gray2 --gray2-strength=12 --soa-tile
```

注意：`--bw1/--gray2` 在 `--soa-ci/--regress-*` 等 headless 模式下会自动禁用。

## 3. Player UI 自动化回放（--ui-ci）

`--ui-ci` 走 **输入回放 + 断言**，用于验证“点击切换/选择”等行为是否正确。

```powershell
g:\Project\Codex\Charm-os\Examples\project\player\win\cmake-build-debug\charm-player-win-vivid-md3.exe --ui-ci
```

输出格式：

```
[ui-ci] case=library_to_now ok=1
[ui-ci] case=now_to_library ok=1
[ui-ci] case=list_select ok=1
[ui-ci] done ok=1 failed=0
```

说明：
- `list_select` 在没有曲目时会显示 `skipped_no_tracks`，整体仍为 ok。
- 失败返回码为 `2`，便于脚本判断。

## 4. 最小复现包建议

提交或传递以下三项即可复现 UI：
1) `*.vcmd`
2) 运行日志（包含 `[soa-ci]` 或 `[ui-ci]` 输出）
3) 对应 commit hash

## 5. 常用排障点

- `ok=0`：优先看 `reason=`
- `img_new_after_lock != 0`：lock 后仍有图片注册
- `cmd_budget`：命令数超预算
- `replay_*` hash 不一致：回放不一致

## 6. Font Contract（CI）

可选参数，使文本/字体行为在 CI 中可预测：

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode ci -RequireFontProvider -RequireFallbackFont -RequireUtf8ReplaceDisabled
```

对应 flags：
- `--require-font-provider`
- `--require-fallback-font`
- `--require-utf8-replace-disabled`
