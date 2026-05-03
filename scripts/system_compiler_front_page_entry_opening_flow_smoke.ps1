param(
    [string]$RouteRoot = "cmake-build-system-compiler-front-page-route-smoke",
    [string]$CapabilityRoot = "cmake-build-system-compiler-front-page-entry-capability-smoke",
    [string]$LandingRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
    [string]$LandingCompareRoot = "cmake-build-system-compiler-front-page-entry-landing-compare-smoke",
    [string]$OpenerRoot = "cmake-build-system-compiler-front-page-entry-opener-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

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

    if (-not (Test-Path $Path)) {
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

    if (Test-Path $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
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
        & $Executable @ArgumentList
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
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

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.Encoding]::UTF8)
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 64
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

function Get-SummaryCount {
    param(
        [string]$Root,
        [string]$Filter
    )

    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path $Root)) {
        return 0
    }

    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $Filter
    ).Count
}

function New-FrontPageSurface {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Role,
        [string]$SummarySchema,
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath
    )

    return [ordered]@{
        id = $Id
        label = $Label
        role = $Role
        summary_schema = $SummarySchema
        summary_path = $SummaryPath
        report_markdown_path = $ReportMarkdownPath
        check_text_path = $CheckTextPath
    }
}

function New-FlowStep {
    param(
        [string]$Id,
        [string]$Label,
        [string]$ScriptPath,
        [string]$OutputRoot,
        [string]$SummaryFilter
    )

    return [ordered]@{
        id = $Id
        label = $Label
        script_path = (Resolve-FullPath -Path $ScriptPath)
        output_root = (Resolve-FullPath -Path $OutputRoot)
        status = "completed"
        produced_summary_count = (Get-SummaryCount -Root $OutputRoot -Filter $SummaryFilter)
    }
}

function New-OpenerCaseRecord {
    param(
        [string]$CaseName,
        [string]$SummaryPath
    )

    $summary = Load-JsonObject -Path $SummaryPath
    $artifactContext = $summary.artifact_context
    $openAction = $summary.open_action
    $openedProjection = $summary.opened_projection
    $compareContext = $summary.compare_context
    $inspectorInvocation = $summary.inspector_invocation

    return [ordered]@{
        name = $CaseName
        summary_path = (Resolve-FullPath -Path $SummaryPath)
        report_markdown_path = (Resolve-FullPath -Path ([string]$artifactContext.report_markdown_path))
        check_text_path = (Resolve-FullPath -Path ([string]$artifactContext.check_text_path))
        open_action_status = [string]$openAction.status
        selected_tab_id = [string]$openAction.selected_tab_id
        selected_role = [string]$openAction.selected_role
        query_kind = [string]$openAction.query_kind
        query_scope = [string]$openAction.query_scope
        selection_rule = [string]$openAction.selection_rule
        target_summary_schema = [string]$openAction.target_summary_schema
        target_summary_kind = [string]$openAction.target_summary_kind
        target_summary_path = (Resolve-FullPath -Path ([string]$openAction.target_summary_path))
        projection_status = [string]$openedProjection.status
        projection_kind = [string]$openedProjection.projection_kind
        compare_context_available = [bool]$compareContext.available
        landing_verdict = [string]$compareContext.landing_verdict
        inspector_ready = [bool]$inspectorInvocation.ready
        inspector_mode = [string]$inspectorInvocation.mode
        inspector_blockers = [string[]]@($inspectorInvocation.blockers | ForEach-Object { [string]$_ })
    }
}

function Build-OpeningFlowReport {
    param(
        $Summary
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("# System Compiler Front Page Entry Opening Flow") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add(("- Result: ``{0}``" -f [string]$Summary.result)) | Out-Null
    $lines.Add(("- Summary JSON: ``{0}``" -f [string]$Summary.artifact_context.flow_summary_path)) | Out-Null
    $lines.Add(("- Report: ``{0}``" -f [string]$Summary.artifact_context.report_markdown_path)) | Out-Null
    $lines.Add(("- Check: ``{0}``" -f [string]$Summary.artifact_context.check_text_path)) | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## Flow Status") | Out-Null
    $lines.Add(("- openers={0}/{1}" -f [int]$Summary.flow_status.actual_opener_count, [int]$Summary.flow_status.expected_opener_count)) | Out-Null
    $lines.Add(("- projections={0}" -f [int]$Summary.flow_status.available_projection_count)) | Out-Null
    $lines.Add(("- compare_context={0}" -f [int]$Summary.flow_status.compare_context_count)) | Out-Null
    $lines.Add(("- inspector_ready={0} blocked={1}" -f [int]$Summary.flow_status.inspector_ready_count, [int]$Summary.flow_status.blocked_inspector_count)) | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## Flow Steps") | Out-Null
    foreach ($step in @($Summary.flow_steps)) {
        $lines.Add(("- ``{0}`` status=``{1}`` summaries={2} root=``{3}``" -f [string]$step.id, [string]$step.status, [int]$step.produced_summary_count, [string]$step.output_root)) | Out-Null
    }
    $lines.Add("") | Out-Null
    $lines.Add("## Opener Cases") | Out-Null
    foreach ($case in @($Summary.opener_cases)) {
        $lines.Add(("- ``{0}`` tab=``{1}`` query=``{2}/{3}`` projection=``{4}/{5}`` compare=``{6}/{7}`` inspector_ready=``{8}``" -f [string]$case.name, [string]$case.selected_tab_id, [string]$case.query_kind, [string]$case.query_scope, [string]$case.projection_status, [string]$case.projection_kind, [bool]$case.compare_context_available, [string]$case.landing_verdict, [bool]$case.inspector_ready)) | Out-Null
    }
    $lines.Add("") | Out-Null
    $lines.Add("## Questions") | Out-Null
    foreach ($question in @($Summary.questions.compare_questions)) {
        $lines.Add(("- compare: {0}" -f [string]$question)) | Out-Null
    }
    foreach ($question in @($Summary.questions.next_questions)) {
        $lines.Add(("- next: {0}" -f [string]$question)) | Out-Null
    }

    return ($lines -join [Environment]::NewLine) + [Environment]::NewLine
}

function Build-OpeningFlowCheck {
    param(
        $Summary
    )

    $status = $Summary.flow_status
    return (
        @(
            "result: $($Summary.result)",
            "expected_opener_count: $($status.expected_opener_count)",
            "actual_opener_count: $($status.actual_opener_count)",
            "available_projection_count: $($status.available_projection_count)",
            "compare_context_count: $($status.compare_context_count)",
            "inspector_ready_count: $($status.inspector_ready_count)",
            "blocked_inspector_count: $($status.blocked_inspector_count)",
            "completed_step_count: $($status.completed_step_count)",
            "route_root: $($Summary.artifact_context.route_root)",
            "capability_root: $($Summary.artifact_context.capability_root)",
            "landing_root: $($Summary.artifact_context.landing_root)",
            "landing_compare_root: $($Summary.artifact_context.landing_compare_root)",
            "opener_root: $($Summary.artifact_context.opener_root)"
        ) -join [Environment]::NewLine
    ) + [Environment]::NewLine
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$routeRootPath = Resolve-FullPath -Path $RouteRoot
$capabilityRootPath = Resolve-FullPath -Path $CapabilityRoot
$landingRootPath = Resolve-FullPath -Path $LandingRoot
$landingCompareRootPath = Resolve-FullPath -Path $LandingCompareRoot
$openerRootPath = Resolve-FullPath -Path $OpenerRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$summaryPath = Join-Path $outputRootPath "front-page.entry-opening-flow.summary.json"
$reportMarkdownPath = Join-Path $outputRootPath "front-page.entry-opening-flow.report.md"
$checkTextPath = Join-Path $outputRootPath "front-page.entry-opening-flow.check.txt"

if ($Clean) {
    foreach ($path in @($capabilityRootPath, $landingRootPath, $landingCompareRootPath, $openerRootPath, $outputRootPath)) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$capabilitySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_capability_smoke.ps1"
$landingSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_smoke.ps1"
$landingCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_compare_smoke.ps1"
$openerSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opener_smoke.ps1"
$validateFlowScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow.py"
foreach ($requiredPath in @($capabilitySmokeScript, $landingSmokeScript, $landingCompareSmokeScript, $openerSmokeScript, $validateFlowScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$pythonArgs = @()
if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonArgs = @("-PythonExe", $resolvedPythonExe)
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $capabilitySmokeScript,
            "-InputRoot",
            $routeRootPath,
            "-OutputRoot",
            $capabilityRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry capability smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $landingSmokeScript,
            "-InputRoot",
            $capabilityRootPath,
            "-OutputRoot",
            $landingRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry landing smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $landingCompareSmokeScript,
            "-InputRoot",
            $landingRootPath,
            "-OutputRoot",
            $landingCompareRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry landing compare smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $openerSmokeScript,
            "-LandingRoot",
            $landingRootPath,
            "-LandingCompareRoot",
            $landingCompareRootPath,
            "-OutputRoot",
            $openerRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry opener smoke failed"

    $expectedOpeners = @(
        "root-witness",
        "root-world-compare",
        "root-witness-to-root-world-compare",
        "root-world-compare-to-root-witness",
        "witness-ci-shelf",
        "review-provenance",
        "root-witness-supporting-testimony",
        "witness-ci-shelf-shelf-compare",
        "witness-ci-shelf-candidate-shelf",
        "runtime-evidence-sample"
    )

    $openerCases = [System.Collections.Generic.List[object]]::new()
    $supportingSurfaces = [System.Collections.Generic.List[object]]::new()
    $availableProjectionCount = 0
    $compareContextCount = 0
    $inspectorReadyCount = 0
    foreach ($caseName in $expectedOpeners) {
        $openerSummaryPath = Join-Path $openerRootPath "$caseName\front-page.entry-opener.summary.json"
        Assert-Condition `
            -Condition (Test-Path $openerSummaryPath) `
            -Message ("missing opener summary for case '{0}': {1}" -f $caseName, $openerSummaryPath)

        $caseRecord = New-OpenerCaseRecord -CaseName $caseName -SummaryPath $openerSummaryPath
        if ([string]$caseRecord.projection_status -eq "available") {
            $availableProjectionCount += 1
        }
        if ([bool]$caseRecord.compare_context_available) {
            $compareContextCount += 1
        }
        if ([bool]$caseRecord.inspector_ready) {
            $inspectorReadyCount += 1
        }

        $openerCases.Add($caseRecord) | Out-Null
        $supportingSurfaces.Add(
            (New-FrontPageSurface `
                -Id ("opener_{0}" -f $caseName.Replace("-", "_")) `
                -Label ("opener: {0}" -f $caseName) `
                -Role "entry_opener_case" `
                -SummarySchema "system_compiler.front_page_entry_opener/v0" `
                -SummaryPath ([string]$caseRecord.summary_path) `
                -ReportMarkdownPath ([string]$caseRecord.report_markdown_path) `
                -CheckTextPath ([string]$caseRecord.check_text_path))
        ) | Out-Null
    }

    Assert-Condition `
        -Condition ($availableProjectionCount -eq @($expectedOpeners).Count) `
        -Message ("expected all opener cases to expose available projections, got {0}/{1}" -f $availableProjectionCount, @($expectedOpeners).Count)
    Assert-Condition `
        -Condition ($compareContextCount -ge 2) `
        -Message ("expected at least two opener cases with compare context, got {0}" -f $compareContextCount)

    $flowSteps = @(
        (New-FlowStep `
            -Id "entry_capability" `
            -Label "front page entry capability smoke" `
            -ScriptPath $capabilitySmokeScript `
            -OutputRoot $capabilityRootPath `
            -SummaryFilter "front-page.entry-capability.summary.json"),
        (New-FlowStep `
            -Id "entry_landing" `
            -Label "front page entry landing smoke" `
            -ScriptPath $landingSmokeScript `
            -OutputRoot $landingRootPath `
            -SummaryFilter "front-page.entry-landing.summary.json"),
        (New-FlowStep `
            -Id "entry_landing_compare" `
            -Label "front page entry landing compare smoke" `
            -ScriptPath $landingCompareSmokeScript `
            -OutputRoot $landingCompareRootPath `
            -SummaryFilter "front-page.entry-landing.compare.summary.json"),
        (New-FlowStep `
            -Id "entry_opener" `
            -Label "front page entry opener smoke" `
            -ScriptPath $openerSmokeScript `
            -OutputRoot $openerRootPath `
            -SummaryFilter "front-page.entry-opener.summary.json")
    )

    $blockedInspectorCount = @($expectedOpeners).Count - $inspectorReadyCount
    $flowSummary = [ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow/v0"
        kind = "system_compiler.front_page_entry_opening_flow"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        generator = "scripts/system_compiler_front_page_entry_opening_flow_smoke.ps1"
        result = "ok"
        opening_flow = [ordered]@{
            title = "System Compiler Front Page Entry Opening Flow"
            summary = "A smoke-level evidence object proving that capability, landing, landing compare, and opener seams produce deterministic explain opening actions."
        }
        front_page = [ordered]@{
            summary_path = (Resolve-FullPath -Path $summaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reportMarkdownPath)
            check_text_path = (Resolve-FullPath -Path $checkTextPath)
            supporting_surfaces = [object[]]@($supportingSurfaces)
        }
        artifact_context = [ordered]@{
            route_root = $routeRootPath
            capability_root = $capabilityRootPath
            landing_root = $landingRootPath
            landing_compare_root = $landingCompareRootPath
            opener_root = $openerRootPath
            output_root = $outputRootPath
            flow_summary_path = (Resolve-FullPath -Path $summaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reportMarkdownPath)
            check_text_path = (Resolve-FullPath -Path $checkTextPath)
        }
        flow_status = [ordered]@{
            expected_opener_count = @($expectedOpeners).Count
            actual_opener_count = @($openerCases).Count
            available_projection_count = $availableProjectionCount
            compare_context_count = $compareContextCount
            inspector_ready_count = $inspectorReadyCount
            blocked_inspector_count = $blockedInspectorCount
            completed_step_count = @($flowSteps).Count
            result = "ok"
        }
        flow_steps = [object[]]@($flowSteps)
        opener_cases = [object[]]@($openerCases)
        questions = [ordered]@{
            compare_questions = [string[]]@(
                "Do compare-backed opener cases still preserve the candidate/baseline drift context at the opening seam?",
                "Did any opener projection become unavailable after changing the entry landing or query policy?"
            )
            next_questions = [string[]]@(
                "Should the opening flow summary become the default handoff object for explain-surface consumers?",
                "Which opener projection should be promoted into a richer interactive explain surface first?"
            )
        }
        violations = [string[]]@()
    }

    Write-JsonFile -Path $summaryPath -Value $flowSummary
    Write-TextFile -Path $reportMarkdownPath -Content (Build-OpeningFlowReport -Summary $flowSummary)
    Write-TextFile -Path $checkTextPath -Content (Build-OpeningFlowCheck -Summary $flowSummary)

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateFlowScript, "--summary", $summaryPath) `
        -FailureMessage "front page entry opening flow validation failed"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] openers={0} projections={1} compare_context={2} inspector_ready={3}" -f
        @($expectedOpeners).Count,
        $availableProjectionCount,
        $compareContextCount,
        $inspectorReadyCount
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] route_root={0}" -f $routeRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] capability_root={0}" -f $capabilityRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] landing_root={0}" -f $landingRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] landing_compare_root={0}" -f $landingCompareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] opener_root={0}" -f $openerRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] output_root={0}" -f $outputRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] summary={0}" -f (Resolve-FullPath -Path $summaryPath))
