要让 Windows PowerShell 默认使用 UTF-8（避免乱码），可以从以下几个方向配置：

✅ 1. 永久让 PowerShell 默认使用 UTF-8

PowerShell 有一个启动时会加载的配置文件 $PROFILE。把 UTF-8 编码设置写进去后，每次打开 PowerShell 都自动带上：

设置步骤

打开 PowerShell

创建 profile文件（如果还没有）：

New-Item -ItemType File -Path $PROFILE -Force


编辑配置文件：

notepad $PROFILE


在打开的文件末尾加入以下内容：

# 控制台编码设为 UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
# 当前控制台的代码页切换为 65001（UTF-8）
chcp 65001 | Out-Null
# PowerShell 输出编码
$OutputEncoding = [System.Text.Encoding]::UTF8


保存 & 重新打开 PowerShell

这样新开的 PowerShell 窗口默认就是 UTF-8 了。
