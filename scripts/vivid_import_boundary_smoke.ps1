param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$archLint = Join-Path $PSScriptRoot "arch_lint.ps1"
& $archLint -Root $Root -OnlyVividImportBoundary
exit $LASTEXITCODE
