# PowerShell UTF-8 操作说明

> status: `supporting`
>
> scope: 当前 shell 会话中的 UTF-8 显示、读写与严格验证

优先使用 PowerShell 7。不要仅因终端显示乱码就重写文件；先用 Git diff、原始 bytes 或严格 decoder
确认文件是否损坏。

## 当前会话

需要与 native process 交换 UTF-8 时，可在当前会话设置：

```powershell
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
```

旧 Windows PowerShell 与部分 native 工具还依赖 console code page；只在确认需要时对当前会话执行：

```powershell
chcp 65001 | Out-Null
```

不要默认修改用户 `$PROFILE`。全局 profile 会影响其它仓库、脚本和工具，只有用户明确希望永久配置时才
单独处理。

## 文件读取与写入

PowerShell 命令显式使用 `-Encoding UTF8`。需要确保 BOM-free UTF-8 时使用 PowerShell 7，或使用
`UTF8Encoding($false)` 的结构化写入方式。代码 Agent 的手工文件修改继续使用 `apply_patch`，不通过
shell 拼接正文。

严格验证 bytes 是否为 UTF-8：

```powershell
$strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)
$bytes = [System.IO.File]::ReadAllBytes($path)
[void]$strictUtf8.GetString($bytes)
```

该检查只证明编码合法，不证明中文语义正确。修复 mojibake 前还必须找到可信原文或更高权威事实；无法
恢复时标记 damaged 并移出推荐入口，不能猜写正文。
