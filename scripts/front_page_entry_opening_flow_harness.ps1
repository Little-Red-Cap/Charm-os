function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Assert-CleanPath {
    param(
        [string]$Path,
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $RootPath
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $rootPrefix = $resolvedRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if ($resolvedPath.Equals($resolvedRoot, $comparison)) {
        throw "refusing to clean repo root: $resolvedPath"
    }
    if (-not $resolvedPath.StartsWith($rootPrefix, $comparison)) {
        throw "refusing to clean outside repo root: $resolvedPath"
    }
}

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "tool not found: $($Candidates -join ', ')"
}

function Resolve-PythonExe {
    param(
        [string]$PythonExe
    )

    if ([string]::IsNullOrWhiteSpace($PythonExe)) {
        return (Resolve-ToolPath -Candidates @("python.exe", "python"))
    }

    return (Resolve-FullPath -Path $PythonExe)
}

function Resolve-PowerShellExe {
    return (Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh"))
}

function Assert-RequiredPaths {
    param(
        [string[]]$Paths
    )

    foreach ($requiredPath in $Paths) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "missing path: $requiredPath"
        }
    }
}

function Initialize-SmokeOutputRoot {
    param(
        [string]$OutputRootPath,
        [bool]$Clean
    )

    if ($Clean) {
        Remove-PathIfExists -Path $OutputRootPath
    }
    Ensure-Directory -Path $OutputRootPath
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList | ForEach-Object { Write-Host $_ }
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Invoke-PowerShellScript {
    param(
        [string]$PowerShellExe,
        [string]$ScriptPath,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Invoke-ExternalTool `
        -Executable $PowerShellExe `
        -ArgumentList (@(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $ScriptPath
        ) + $ArgumentList) `
        -FailureMessage $FailureMessage
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $Content
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 100
    Write-TextFile -Path $Path -Content ($json + [Environment]::NewLine)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-SingleSelector {
    param(
        [string]$Label,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName
    )

    $selectorCount = 0
    foreach ($value in @($ActionId, $ActionKind, $EntryName)) {
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $selectorCount += 1
        }
    }

    if ($selectorCount -gt 1) {
        throw ("{0}: use only one of ActionId, ActionKind, or EntryName" -f $Label)
    }
}

function Ensure-OpeningFlowConsumerPlanActionWorkspaceSmoke {
    param(
        [string]$ScriptsRoot,
        [string]$ActionWorkspaceRootPath,
        [string]$PythonExe,
        [string]$PowerShellExe,
        [bool]$Clean,
        [string]$LogPrefix
    )

    $defaultActionPath = Join-Path $ActionWorkspaceRootPath "cold-default\action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $defaultActionPath)) {
        Write-Host ("{0} action_bootstrap=reuse-existing" -f $LogPrefix)
        return $defaultActionPath
    }

    $actionWorkspaceSmokeScript = Join-Path $ScriptsRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
    Assert-RequiredPaths -Paths @($actionWorkspaceSmokeScript)
    Invoke-PowerShellScript `
        -PowerShellExe $PowerShellExe `
        -ScriptPath $actionWorkspaceSmokeScript `
        -ArgumentList @(
            "-OutputRoot",
            $ActionWorkspaceRootPath,
            "-PythonExe",
            $PythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action workspace smoke bootstrap failed"

    return $defaultActionPath
}

function Ensure-OpeningFlowConsumerPlanActionCompareSmoke {
    param(
        [string]$ScriptsRoot,
        [string]$ActionWorkspaceRootPath,
        [string]$ActionCompareRootPath,
        [string]$PythonExe,
        [string]$PowerShellExe,
        [bool]$Clean,
        [string]$LogPrefix
    )

    $compareSummaryPath = Join-Path $ActionCompareRootPath "default-to-compare-neighbor\front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $compareSummaryPath)) {
        Write-Host ("{0} compare_bootstrap=reuse-existing" -f $LogPrefix)
        return $compareSummaryPath
    }

    $actionCompareSmokeScript = Join-Path $ScriptsRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare_smoke.ps1"
    Assert-RequiredPaths -Paths @($actionCompareSmokeScript)
    Invoke-PowerShellScript `
        -PowerShellExe $PowerShellExe `
        -ScriptPath $actionCompareSmokeScript `
        -ArgumentList @(
            "-ActionWorkspaceRoot",
            $ActionWorkspaceRootPath,
            "-OutputRoot",
            $ActionCompareRootPath,
            "-PythonExe",
            $PythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action compare smoke bootstrap failed"

    return $compareSummaryPath
}

function Resolve-OpeningFlowOpenEventSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $WorkspaceRoot "open-event\front-page.entry-opening-flow.open-event.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opening-flow.open-event.summary.json")
}

function Resolve-OpeningFlowConsumerPlanActionSummaryPath {
    param(
        [string]$ActionWorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $ActionWorkspaceRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $ActionWorkspaceRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json")
}

function Resolve-OpeningFlowOpenEventWitnessSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $WorkspaceRoot "open-event-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opening-flow.open-event.witness.summary.json")
}
