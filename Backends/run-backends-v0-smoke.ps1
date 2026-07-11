$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()

Write-Host '[backends-v0] compatibility wrapper: running Backends v1 gate'
& (Join-Path $PSScriptRoot 'run-backends-v1-smoke.ps1')
