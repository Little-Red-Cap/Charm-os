param(
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"

$SmokeNames = @(
    "rte_component_context_smoke",
    "rte_init_projection_smoke",
    "rte_profile_materialization_smoke",
    "rte_profile_resolution_smoke",
    "rte_projection_gate_smoke",
    "rte_projection_consistency_smoke",
    "rte_explain_projection_smoke",
    "rte_context_slice_smoke",
    "rte_evidence_slice_smoke",
    "rte_multi_role_provider_smoke",
    "rte_profile_selection_smoke",
    "charm_spine_smoke",
    "charm_spine_evidence_projection_smoke"
)

$Results = @()

foreach ($Name in $SmokeNames) {
    $SourceDir = Join-Path $PSScriptRoot $Name
    $BuildDir = Join-Path $SourceDir ("cmake-build-" + $Name.Replace("_", "-"))

    Write-Host "=== $Name ==="

    $ConfigureExit = -1
    $BuildExit = -1
    $TestExit = -1

    if (-not (Test-Path -LiteralPath $SourceDir)) {
        Write-Host "Missing smoke directory: $SourceDir"
    } else {
        & cmake -S $SourceDir -B $BuildDir -G $Generator
        $ConfigureExit = $LASTEXITCODE

        if ($ConfigureExit -eq 0) {
            & cmake --build $BuildDir
            $BuildExit = $LASTEXITCODE
        }

        if (($ConfigureExit -eq 0) -and ($BuildExit -eq 0)) {
            & ctest --test-dir $BuildDir --output-on-failure
            $TestExit = $LASTEXITCODE
        }
    }

    $Results += [pscustomobject]@{
        Name = $Name
        Configure = $ConfigureExit
        Build = $BuildExit
        Test = $TestExit
    }
}

Write-Host "=== summary ==="
$Results | Format-Table -AutoSize

$Failed = $Results | Where-Object {
    ($_.Configure -ne 0) -or ($_.Build -ne 0) -or ($_.Test -ne 0)
}

if ($Failed) {
    exit 1
}

exit 0
