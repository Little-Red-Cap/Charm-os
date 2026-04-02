param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\.." ).Path,
    [switch]$Staged
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Command([string]$Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-DiffText {
    if ($Staged) {
        return & git diff --cached -U0
    }
    return & git diff -U0
}

function Get-ChangedFiles {
    if ($Staged) {
        return & git diff --cached --name-only
    }
    return & git diff --name-only
}

if (-not (Test-Command "git")) {
    Write-Host "[ssu_submit_gate] git not found; skipping."
    exit 0
}

$diff = Get-DiffText
if (-not $diff) {
    Write-Host "[ssu_submit_gate] no diff; skipping."
    exit 0
}

$triggerPattern = 'post_demand\b|post_io_ready\b|post_demand_token\b|post_io_ready_token\b|SubmitProjection::|add_step\(|format_audit_json\b'
$addedLines = $diff | Where-Object { $_ -match '^\+' -and $_ -notmatch '^\+\+\+' }
$hits = $addedLines | Where-Object { $_ -match $triggerPattern }

if (-not $hits) {
    Write-Host "[ssu_submit_gate] OK (no submit-mapping changes detected)"
    exit 0
}

$files = Get-ChangedFiles
$docPattern = '^docs/system/(ssu_submit_inventory|ssu_run_loop_audit|ssu_submit_discipline)\.md$'
$docHits = $files | Where-Object { $_ -match $docPattern }

if (-not $docHits) {
    Write-Host ""
    Write-Host "[ssu_submit_gate] FAILED"
    Write-Host "  Detected submit-mapping related changes, but no required docs updated."
    Write-Host "  Update one of:"
    Write-Host "    - docs/system/ssu_submit_inventory.md"
    Write-Host "    - docs/system/ssu_run_loop_audit.md"
    Write-Host "    - docs/system/ssu_submit_discipline.md"
    Write-Host ""
    Write-Host "  Matched lines:"
    foreach ($line in $hits) {
        Write-Host "    $line"
    }
    exit 1
}

Write-Host "[ssu_submit_gate] OK (submit-mapping change documented)"
exit 0
