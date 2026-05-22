param(
    [string]$SampleRoot = "schemas/examples",
    [string]$PythonExe = "python"
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$staticRefsSmoke = Join-Path $PSScriptRoot "schema_examples_static_refs_smoke.ps1"
if (-not (Test-Path -LiteralPath $staticRefsSmoke -PathType Leaf)) {
    throw "required smoke not found: $staticRefsSmoke"
}

Write-Host "[SCHEMA-EXAMPLES-HYGIENE-SMOKE] static_refs=running"
& $staticRefsSmoke -SampleRoot $SampleRoot -PythonExe $PythonExe

Write-Host "[SCHEMA-EXAMPLES-HYGIENE-SMOKE] result=ok"
