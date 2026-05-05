param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-sample-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    Remove-Item -LiteralPath $Path -Recurse -Force
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

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
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

function Select-RuntimeSessionPrimaryLanding {
    param(
        [string]$SourceLandingSummaryPath,
        [string]$OutputDirectory
    )

    Ensure-Directory -Path $OutputDirectory

    $landing = Load-JsonObject -Path $SourceLandingSummaryPath
    $runtimeSessionTab = @($landing.landing_tabs) |
        Where-Object { [string]$_.tab_id -eq "runtime_session" } |
        Select-Object -First 1
    $runtimeSessionQuery = @($landing.query_hints.tab_queries) |
        Where-Object { [string]$_.tab_id -eq "runtime_session" } |
        Select-Object -First 1
    Assert-Condition `
        -Condition ($runtimeSessionTab -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionTab -is [hashtable]) `
        -Message "runtime_session landing tab not found"
    Assert-Condition `
        -Condition ($runtimeSessionQuery -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionQuery -is [hashtable]) `
        -Message "runtime_session query hint not found"

    $summaryPath = Join-Path $OutputDirectory "front-page.entry-landing.summary.json"
    $reportPath = Join-Path $OutputDirectory "front-page.entry-landing.report.md"
    $checkPath = Join-Path $OutputDirectory "front-page.entry-landing.check.txt"

    $sourceReport = [string]$landing.artifact_context.report_markdown_path
    $sourceCheck = [string]$landing.artifact_context.check_text_path
    if (Test-Path $sourceReport) {
        Copy-Item -LiteralPath $sourceReport -Destination $reportPath -Force
    } else {
        Write-TextFile -Path $reportPath -Content "# Runtime Session Primary Landing`n"
    }
    if (Test-Path $sourceCheck) {
        Copy-Item -LiteralPath $sourceCheck -Destination $checkPath -Force
    } else {
        Write-TextFile -Path $checkPath -Content "runtime_session_primary: true`n"
    }

    $landing.artifact_context.output_root = $OutputDirectory
    $landing.artifact_context.landing_summary_path = $summaryPath
    $landing.artifact_context.report_markdown_path = $reportPath
    $landing.artifact_context.check_text_path = $checkPath
    $landing.front_page.summary_path = $summaryPath
    $landing.front_page.report_markdown_path = $reportPath
    $landing.front_page.check_text_path = $checkPath
    $landing.primary_landing = $runtimeSessionTab
    $landing.query_hints.primary_query = $runtimeSessionQuery
    $landing.landing_status.primary_tab_id = "runtime_session"
    $landing.landing_status.primary_summary_schema = [string]$runtimeSessionTab.entry.summary_schema
    $landing.landing_status.primary_summary_kind = [string]$runtimeSessionTab.entry.summary_kind

    Write-JsonFile -Path $summaryPath -Value $landing
    return $summaryPath
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
    $questions = $summary.questions

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
        opening_reason = $openAction.opening_reason
        target_summary_schema = [string]$openAction.target_summary_schema
        target_summary_kind = [string]$openAction.target_summary_kind
        target_summary_path = (Resolve-FullPath -Path ([string]$openAction.target_summary_path))
        open_action_blockers = [string[]]@($openAction.blockers | ForEach-Object { [string]$_ })
        projection_status = [string]$openedProjection.status
        projection_kind = [string]$openedProjection.projection_kind
        projection_headline = [string]$openedProjection.headline
        projection_summary_lines = [string[]]@($openedProjection.summary_lines | ForEach-Object { [string]$_ })
        projection_question_lines = [string[]]@($openedProjection.question_lines | ForEach-Object { [string]$_ })
        projection_blockers = [string[]]@($openedProjection.blockers | ForEach-Object { [string]$_ })
        compare_context_available = [bool]$compareContext.available
        landing_verdict = [string]$compareContext.landing_verdict
        inspector_ready = [bool]$inspectorInvocation.ready
        inspector_mode = [string]$inspectorInvocation.mode
        inspector_blockers = [string[]]@($inspectorInvocation.blockers | ForEach-Object { [string]$_ })
        opener_compare_questions = [string[]]@($questions.compare_questions | ForEach-Object { [string]$_ })
        opener_next_questions = [string[]]@($questions.next_questions | ForEach-Object { [string]$_ })
    }
}

function Build-OpeningFlowReport {
    param(
        $Summary
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("# System Compiler Front Page Entry Runtime Session Opening Flow") | Out-Null
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
        if (-not [string]::IsNullOrWhiteSpace([string]$case.projection_headline)) {
            $lines.Add(("  preview: {0}" -f [string]$case.projection_headline)) | Out-Null
        }
        foreach ($item in @($case.projection_summary_lines | Select-Object -First 3)) {
            $lines.Add(("  summary: {0}" -f [string]$item)) | Out-Null
        }
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
            "runtime_session_projection_count: $(@($Summary.opener_cases | Where-Object { [string]$_.projection_kind -eq 'kernel_runtime_session_overview' }).Count)",
            "route_root: $($Summary.artifact_context.route_root)",
            "capability_root: $($Summary.artifact_context.capability_root)",
            "landing_root: $($Summary.artifact_context.landing_root)",
            "landing_compare_root: $($Summary.artifact_context.landing_compare_root)",
            "opener_root: $($Summary.artifact_context.opener_root)"
        ) -join [Environment]::NewLine
    ) + [Environment]::NewLine
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$sampleSummaryPath = Resolve-FullPath -Path $SampleSummary
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $sampleSummaryPath)) {
    throw "sample summary not found: $sampleSummaryPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$routeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
$capabilityExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_capability.py"
$capabilityValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_capability.py"
$landingExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_landing.py"
$landingValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing.py"
$openerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$openerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
$flowValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow.py"
foreach ($requiredPath in @(
    $routeExportScript,
    $routeValidateScript,
    $capabilityExportScript,
    $capabilityValidateScript,
    $landingExportScript,
    $landingValidateScript,
    $openerExportScript,
    $openerValidateScript,
    $flowValidateScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $routeOutputRoot = Join-Path $outputRootPath "route"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($routeExportScript, "--summary", $sampleSummaryPath, "--output-root", $routeOutputRoot) `
        -FailureMessage "front page route sample export failed"

    $routeSummaryPath = Join-Path $routeOutputRoot "front-page.route.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($routeValidateScript, "--summary", $routeSummaryPath) `
        -FailureMessage "front page route sample validation failed"

    $capabilityOutputRoot = Join-Path $outputRootPath "capability"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityExportScript, "--summary", $routeSummaryPath, "--output-root", $capabilityOutputRoot) `
        -FailureMessage "front page entry capability sample export failed"

    $capabilitySummaryPath = Join-Path $capabilityOutputRoot "front-page.entry-capability.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityValidateScript, "--summary", $capabilitySummaryPath) `
        -FailureMessage "front page entry capability sample validation failed"

    $landingOutputRoot = Join-Path $outputRootPath "landing"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingExportScript, "--summary", $capabilitySummaryPath, "--output-root", $landingOutputRoot) `
        -FailureMessage "front page entry landing sample export failed"

    $landingSummaryPath = Join-Path $landingOutputRoot "front-page.entry-landing.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingValidateScript, "--summary", $landingSummaryPath) `
        -FailureMessage "front page entry landing sample validation failed"

    $runtimeSessionLandingPath = Select-RuntimeSessionPrimaryLanding `
        -SourceLandingSummaryPath $landingSummaryPath `
        -OutputDirectory (Join-Path $outputRootPath "runtime-session-landing")

    $openerOutputRoot = Join-Path $outputRootPath "opener\runtime-session-sample"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openerExportScript, "--landing", $runtimeSessionLandingPath, "--output-root", $openerOutputRoot) `
        -FailureMessage "front page entry opener runtime session sample export failed"

    $openerSummaryPath = Join-Path $openerOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openerValidateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener runtime session sample validation failed"

    $landingCompareRoot = Join-Path $outputRootPath "landing-compare-unused"
    Ensure-Directory -Path $landingCompareRoot
    Write-TextFile -Path (Join-Path $landingCompareRoot "README.txt") -Content "runtime_session sample flow does not require a landing compare artifact.`n"

    $summaryPath = Join-Path $outputRootPath "front-page.entry-opening-flow.summary.json"
    $reportMarkdownPath = Join-Path $outputRootPath "front-page.entry-opening-flow.report.md"
    $checkTextPath = Join-Path $outputRootPath "front-page.entry-opening-flow.check.txt"

    $openerCase = New-OpenerCaseRecord -CaseName "runtime-session-sample" -SummaryPath $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerCase.selected_tab_id -eq "runtime_session") `
        -Message ("expected selected_tab_id runtime_session but got '{0}'" -f $openerCase.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$openerCase.query_kind -eq "bringup_evidence") `
        -Message ("expected query_kind bringup_evidence but got '{0}'" -f $openerCase.query_kind)
    Assert-Condition `
        -Condition ([string]$openerCase.projection_status -eq "available") `
        -Message ("expected projection status available but got '{0}'" -f $openerCase.projection_status)
    Assert-Condition `
        -Condition ([string]$openerCase.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected projection kind kernel_runtime_session_overview but got '{0}'" -f $openerCase.projection_kind)

    $flowSteps = @(
        (New-FlowStep `
            -Id "front_page_route" `
            -Label "front page route sample export" `
            -ScriptPath $routeExportScript `
            -OutputRoot $routeOutputRoot `
            -SummaryFilter "front-page.route.summary.json"),
        (New-FlowStep `
            -Id "entry_capability" `
            -Label "front page entry capability sample export" `
            -ScriptPath $capabilityExportScript `
            -OutputRoot $capabilityOutputRoot `
            -SummaryFilter "front-page.entry-capability.summary.json"),
        (New-FlowStep `
            -Id "entry_landing" `
            -Label "front page entry landing sample export" `
            -ScriptPath $landingExportScript `
            -OutputRoot $landingOutputRoot `
            -SummaryFilter "front-page.entry-landing.summary.json"),
        (New-FlowStep `
            -Id "entry_opener" `
            -Label "front page entry opener runtime session sample export" `
            -ScriptPath $openerExportScript `
            -OutputRoot $openerOutputRoot `
            -SummaryFilter "front-page.entry-opener.summary.json")
    )

    $openerCases = [object[]]@($openerCase)
    $availableProjectionCount = @($openerCases | Where-Object { [string]$_.projection_status -eq "available" }).Count
    $compareContextCount = @($openerCases | Where-Object { [bool]$_.compare_context_available }).Count
    $inspectorReadyCount = @($openerCases | Where-Object { [bool]$_.inspector_ready }).Count
    $blockedInspectorCount = @($openerCases).Count - $inspectorReadyCount
    Assert-Condition `
        -Condition ($availableProjectionCount -eq 1) `
        -Message ("expected available_projection_count=1 but got {0}" -f $availableProjectionCount)

    $supportingSurfaces = [object[]]@(
        (New-FrontPageSurface `
            -Id "opener_runtime_session_sample" `
            -Label "opener: runtime-session-sample" `
            -Role "entry_opener_case" `
            -SummarySchema "system_compiler.front_page_entry_opener/v0" `
            -SummaryPath ([string]$openerCase.summary_path) `
            -ReportMarkdownPath ([string]$openerCase.report_markdown_path) `
            -CheckTextPath ([string]$openerCase.check_text_path))
    )

    $flowSummary = [ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow/v0"
        kind = "system_compiler.front_page_entry_opening_flow"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        generator = "scripts/system_compiler_front_page_entry_runtime_session_opening_flow_sample_smoke.ps1"
        result = "ok"
        opening_flow = [ordered]@{
            title = "System Compiler Front Page Entry Runtime Session Opening Flow"
            summary = "A narrow smoke artifact proving that runtime_session can travel from route/capability/landing/opener into the opening-flow handoff shape."
        }
        front_page = [ordered]@{
            summary_path = (Resolve-FullPath -Path $summaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reportMarkdownPath)
            check_text_path = (Resolve-FullPath -Path $checkTextPath)
            supporting_surfaces = [object[]]@($supportingSurfaces)
        }
        artifact_context = [ordered]@{
            route_root = (Resolve-FullPath -Path $routeOutputRoot)
            capability_root = (Resolve-FullPath -Path $capabilityOutputRoot)
            landing_root = (Resolve-FullPath -Path $landingOutputRoot)
            landing_compare_root = (Resolve-FullPath -Path $landingCompareRoot)
            opener_root = (Resolve-FullPath -Path $openerOutputRoot)
            output_root = (Resolve-FullPath -Path $outputRootPath)
            flow_summary_path = (Resolve-FullPath -Path $summaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reportMarkdownPath)
            check_text_path = (Resolve-FullPath -Path $checkTextPath)
        }
        flow_status = [ordered]@{
            expected_opener_count = 1
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
            compare_questions = [string[]]@()
            next_questions = [string[]]@(
                "Should runtime_session be promoted from a narrow sample case into the full multi-case opening flow once the heavy workspace is available?",
                "Which downstream consumer should open kernel_runtime_session_overview first?"
            )
        }
        violations = [string[]]@()
    }

    Write-JsonFile -Path $summaryPath -Value $flowSummary
    Write-TextFile -Path $reportMarkdownPath -Content (Build-OpeningFlowReport -Summary $flowSummary)
    Write-TextFile -Path $checkTextPath -Content (Build-OpeningFlowCheck -Summary $flowSummary)

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($flowValidateScript, "--summary", $summaryPath) `
        -FailureMessage "front page entry runtime session opening flow sample validation failed"

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENING-FLOW-SAMPLE-SMOKE] landing={0}" -f $runtimeSessionLandingPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENING-FLOW-SAMPLE-SMOKE] opener={0}" -f $openerSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENING-FLOW-SAMPLE-SMOKE] flow={0}" -f $summaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENING-FLOW-SAMPLE-SMOKE] projection={0}/{1}" -f $openerCase.projection_status, $openerCase.projection_kind)
    Write-Host "ok=1"
} finally {
    Pop-Location
}
