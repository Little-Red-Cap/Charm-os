param(
    [Parameter(Mandatory = $true)]
    [string]$SummaryPath,
    [string]$RuntimeEvidenceSummary = "",
    [string]$BiographySummary = "",
    [string]$WorldCompareSummary = "",
    [string]$WorldShelfReviewSummary = ""
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if ([string]::IsNullOrWhiteSpace($parent)) {
        return
    }

    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name
    )

    if ($null -eq $Object -or [string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return $Object[$Name]
        }

        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property) {
        return $property.Value
    }

    return $null
}

function Set-ObjectProperty {
    param(
        $Object,
        [string]$Name,
        $Value
    )

    if ($null -eq $Object) {
        throw "cannot set property '$Name' on null object"
    }

    if ($Object -is [System.Collections.IDictionary]) {
        $Object[$Name] = $Value
        return
    }

    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

function Get-FirstNonEmptyString {
    param(
        [object[]]$Values
    )

    foreach ($value in @($Values)) {
        if ($null -eq $value) {
            continue
        }

        $text = [string]$value
        if (-not [string]::IsNullOrWhiteSpace($text)) {
            return $text
        }
    }

    return ""
}

function Resolve-ReferencedPath {
    param(
        [string]$BasePath,
        [string]$Value,
        [string]$FallbackPath = ""
    )

    $selectedValue = Get-FirstNonEmptyString -Values @($Value, $FallbackPath)
    if ([string]::IsNullOrWhiteSpace($selectedValue)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($selectedValue)) {
        return [System.IO.Path]::GetFullPath($selectedValue)
    }

    if ([string]::IsNullOrWhiteSpace($BasePath)) {
        return Resolve-FullPath -Path $selectedValue
    }

    $baseDirectory = Split-Path -Parent $BasePath
    return [System.IO.Path]::GetFullPath((Join-Path $baseDirectory $selectedValue))
}

function Require-Schema {
    param(
        $Summary,
        [string]$Path,
        [string]$ExpectedSchema
    )

    $actualSchema = [string](Get-ObjectPropertyValue -Object $Summary -Name "schema")
    if ($actualSchema -ne $ExpectedSchema) {
        throw ("schema mismatch for {0}: expected {1}, got {2}" -f $Path, $ExpectedSchema, $actualSchema)
    }
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

function Resolve-SummarySurface {
    param(
        [string]$SummaryPath,
        [string]$ExpectedSchema
    )

    if ([string]::IsNullOrWhiteSpace($SummaryPath)) {
        return $null
    }

    $resolvedSummaryPath = Resolve-FullPath -Path $SummaryPath
    if (-not (Test-Path $resolvedSummaryPath)) {
        throw "summary not found: $resolvedSummaryPath"
    }

    $summary = Load-JsonObject -Path $resolvedSummaryPath
    Require-Schema -Summary $summary -Path $resolvedSummaryPath -ExpectedSchema $ExpectedSchema

    $frontPage = Get-ObjectPropertyValue -Object $summary -Name "front_page"
    $delivery = Get-ObjectPropertyValue -Object $summary -Name "delivery"
    $artifactContext = Get-ObjectPropertyValue -Object $summary -Name "artifact_context"

    $resolvedOwnSummaryPath = Resolve-ReferencedPath `
        -BasePath $resolvedSummaryPath `
        -Value (Get-FirstNonEmptyString -Values @(
            (Get-ObjectPropertyValue -Object $frontPage -Name "summary_path"),
            (Get-ObjectPropertyValue -Object $delivery -Name "summary_path"),
            (Get-ObjectPropertyValue -Object $artifactContext -Name "summary_path")
        )) `
        -FallbackPath $resolvedSummaryPath

    $resolvedReportMarkdownPath = Resolve-ReferencedPath `
        -BasePath $resolvedSummaryPath `
        -Value (Get-FirstNonEmptyString -Values @(
            (Get-ObjectPropertyValue -Object $frontPage -Name "report_markdown_path"),
            (Get-ObjectPropertyValue -Object $delivery -Name "report_markdown_path"),
            (Get-ObjectPropertyValue -Object $artifactContext -Name "report_markdown_path"),
            (Get-ObjectPropertyValue -Object $summary -Name "report_markdown_path")
        ))

    $resolvedCheckTextPath = Resolve-ReferencedPath `
        -BasePath $resolvedSummaryPath `
        -Value (Get-FirstNonEmptyString -Values @(
            (Get-ObjectPropertyValue -Object $frontPage -Name "check_text_path"),
            (Get-ObjectPropertyValue -Object $delivery -Name "check_text_path"),
            (Get-ObjectPropertyValue -Object $artifactContext -Name "check_text_path"),
            (Get-ObjectPropertyValue -Object $summary -Name "check_text_path")
        ))

    if ([string]::IsNullOrWhiteSpace($resolvedReportMarkdownPath)) {
        throw "report_markdown_path not found in summary: $resolvedSummaryPath"
    }

    if ([string]::IsNullOrWhiteSpace($resolvedCheckTextPath)) {
        throw "check_text_path not found in summary: $resolvedSummaryPath"
    }

    return [ordered]@{
        Schema = $ExpectedSchema
        SummaryPath = $resolvedOwnSummaryPath
        ReportMarkdownPath = $resolvedReportMarkdownPath
        CheckTextPath = $resolvedCheckTextPath
    }
}

function Resolve-KernelRuntimeSessionSurface {
    param(
        [string]$RuntimeEvidenceSummaryPath
    )

    if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummaryPath)) {
        return $null
    }

    $resolvedRuntimeEvidenceSummaryPath = Resolve-FullPath -Path $RuntimeEvidenceSummaryPath
    if (-not (Test-Path $resolvedRuntimeEvidenceSummaryPath)) {
        throw "runtime evidence summary not found: $resolvedRuntimeEvidenceSummaryPath"
    }

    $runtimeSummary = Load-JsonObject -Path $resolvedRuntimeEvidenceSummaryPath
    Require-Schema `
        -Summary $runtimeSummary `
        -Path $resolvedRuntimeEvidenceSummaryPath `
        -ExpectedSchema "minimal_kernel.runtime_evidence_bundle.summary/v1"

    $sessionView = Get-ObjectPropertyValue -Object $runtimeSummary -Name "session"
    if ($null -eq $sessionView) {
        $sessionView = Get-ObjectPropertyValue -Object $runtimeSummary -Name "session_summary"
    }

    if ($null -eq $sessionView) {
        return $null
    }

    $sessionSummaryPath = Resolve-ReferencedPath `
        -BasePath $resolvedRuntimeEvidenceSummaryPath `
        -Value (Get-ObjectPropertyValue -Object $sessionView -Name "summary_path")
    if ([string]::IsNullOrWhiteSpace($sessionSummaryPath)) {
        return $null
    }
    if (-not (Test-Path $sessionSummaryPath)) {
        throw "kernel runtime session summary not found: $sessionSummaryPath"
    }

    $sessionSummary = Load-JsonObject -Path $sessionSummaryPath
    Require-Schema `
        -Summary $sessionSummary `
        -Path $sessionSummaryPath `
        -ExpectedSchema "minimal_kernel.kernel_runtime_session/v0"

    $artifactPaths = Get-ObjectPropertyValue -Object $sessionSummary -Name "artifact_paths"
    $reportMarkdownPath = Resolve-ReferencedPath `
        -BasePath $resolvedRuntimeEvidenceSummaryPath `
        -Value (Get-ObjectPropertyValue -Object $sessionView -Name "report_markdown_path")
    if ([string]::IsNullOrWhiteSpace($reportMarkdownPath)) {
        $reportMarkdownPath = Resolve-ReferencedPath `
            -BasePath $sessionSummaryPath `
            -Value (Get-ObjectPropertyValue -Object $artifactPaths -Name "report")
    }

    $checkTextPath = Resolve-ReferencedPath `
        -BasePath $resolvedRuntimeEvidenceSummaryPath `
        -Value (Get-ObjectPropertyValue -Object $sessionView -Name "check_text_path")
    if ([string]::IsNullOrWhiteSpace($checkTextPath)) {
        $checkTextPath = Resolve-ReferencedPath `
            -BasePath $sessionSummaryPath `
            -Value (Get-ObjectPropertyValue -Object $artifactPaths -Name "check")
    }

    if ([string]::IsNullOrWhiteSpace($reportMarkdownPath)) {
        $reportMarkdownPath = Join-Path (Split-Path -Parent $sessionSummaryPath) "report.md"
    }

    if ([string]::IsNullOrWhiteSpace($checkTextPath)) {
        $checkTextPath = Join-Path (Split-Path -Parent $sessionSummaryPath) "check.txt"
    }

    return [ordered]@{
        Schema = "minimal_kernel.kernel_runtime_session/v0"
        SummaryPath = $sessionSummaryPath
        ReportMarkdownPath = $reportMarkdownPath
        CheckTextPath = $checkTextPath
    }
}

$resolvedRootSummaryPath = Resolve-FullPath -Path $SummaryPath
if (-not (Test-Path $resolvedRootSummaryPath)) {
    throw "witness bundle summary not found: $resolvedRootSummaryPath"
}

$rootSummary = Load-JsonObject -Path $resolvedRootSummaryPath
Require-Schema -Summary $rootSummary -Path $resolvedRootSummaryPath -ExpectedSchema "system_compiler.witness_bundle/v0"

$rootFrontPage = Get-ObjectPropertyValue -Object $rootSummary -Name "front_page"
$rootDelivery = Get-ObjectPropertyValue -Object $rootSummary -Name "delivery"
$rootArtifactContext = Get-ObjectPropertyValue -Object $rootSummary -Name "artifact_context"

$resolvedFrontPageSummaryPath = Resolve-ReferencedPath `
    -BasePath $resolvedRootSummaryPath `
    -Value (Get-FirstNonEmptyString -Values @(
        (Get-ObjectPropertyValue -Object $rootFrontPage -Name "summary_path"),
        (Get-ObjectPropertyValue -Object $rootDelivery -Name "summary_path"),
        (Get-ObjectPropertyValue -Object $rootArtifactContext -Name "summary_path")
    )) `
    -FallbackPath $resolvedRootSummaryPath

$resolvedFrontPageReportMarkdownPath = Resolve-ReferencedPath `
    -BasePath $resolvedRootSummaryPath `
    -Value (Get-FirstNonEmptyString -Values @(
        (Get-ObjectPropertyValue -Object $rootFrontPage -Name "report_markdown_path"),
        (Get-ObjectPropertyValue -Object $rootDelivery -Name "report_markdown_path"),
        (Get-ObjectPropertyValue -Object $rootArtifactContext -Name "report_markdown_path"),
        (Get-ObjectPropertyValue -Object $rootSummary -Name "report_markdown_path")
    ))

$resolvedFrontPageCheckTextPath = Resolve-ReferencedPath `
    -BasePath $resolvedRootSummaryPath `
    -Value (Get-FirstNonEmptyString -Values @(
        (Get-ObjectPropertyValue -Object $rootFrontPage -Name "check_text_path"),
        (Get-ObjectPropertyValue -Object $rootDelivery -Name "check_text_path"),
        (Get-ObjectPropertyValue -Object $rootArtifactContext -Name "check_text_path"),
        (Get-ObjectPropertyValue -Object $rootSummary -Name "check_text_path")
    ))

if ([string]::IsNullOrWhiteSpace($resolvedFrontPageReportMarkdownPath)) {
    throw "front_page report_markdown_path not found in root witness bundle: $resolvedRootSummaryPath"
}

if ([string]::IsNullOrWhiteSpace($resolvedFrontPageCheckTextPath)) {
    throw "front_page check_text_path not found in root witness bundle: $resolvedRootSummaryPath"
}

$supportingSurfaces = [System.Collections.Generic.List[object]]::new()

$resolvedWorldShelfReviewSurface = Resolve-SummarySurface `
    -SummaryPath $WorldShelfReviewSummary `
    -ExpectedSchema "system_compiler.world_shelf_review/v0"
if ($null -ne $resolvedWorldShelfReviewSurface) {
    $supportingSurfaces.Add(
        (New-FrontPageSurface `
            -Id "world_shelf_review" `
            -Label "world shelf review" `
            -Role "grouped_review" `
            -SummarySchema "system_compiler.world_shelf_review/v0" `
            -SummaryPath $resolvedWorldShelfReviewSurface.SummaryPath `
            -ReportMarkdownPath $resolvedWorldShelfReviewSurface.ReportMarkdownPath `
            -CheckTextPath $resolvedWorldShelfReviewSurface.CheckTextPath)
    ) | Out-Null
}

$resolvedBiographySurface = Resolve-SummarySurface `
    -SummaryPath $BiographySummary `
    -ExpectedSchema "system_compiler.biography/v0"
if ($null -ne $resolvedBiographySurface) {
    $supportingSurfaces.Add(
        (New-FrontPageSurface `
            -Id "biography" `
            -Label "system compiler biography" `
            -Role "delivery_biography" `
            -SummarySchema "system_compiler.biography/v0" `
            -SummaryPath $resolvedBiographySurface.SummaryPath `
            -ReportMarkdownPath $resolvedBiographySurface.ReportMarkdownPath `
            -CheckTextPath $resolvedBiographySurface.CheckTextPath)
    ) | Out-Null
}

$resolvedWorldCompareSurface = Resolve-SummarySurface `
    -SummaryPath $WorldCompareSummary `
    -ExpectedSchema "system_compiler.world_compare/v0"
if ($null -ne $resolvedWorldCompareSurface) {
    $supportingSurfaces.Add(
        (New-FrontPageSurface `
            -Id "world_compare" `
            -Label "system compiler world compare" `
            -Role "counterfactual_verdict" `
            -SummarySchema "system_compiler.world_compare/v0" `
            -SummaryPath $resolvedWorldCompareSurface.SummaryPath `
            -ReportMarkdownPath $resolvedWorldCompareSurface.ReportMarkdownPath `
            -CheckTextPath $resolvedWorldCompareSurface.CheckTextPath)
    ) | Out-Null
}

$resolvedRuntimeEvidenceSurface = Resolve-SummarySurface `
    -SummaryPath $RuntimeEvidenceSummary `
    -ExpectedSchema "minimal_kernel.runtime_evidence_bundle.summary/v1"
if ($null -ne $resolvedRuntimeEvidenceSurface) {
    $supportingSurfaces.Add(
        (New-FrontPageSurface `
            -Id "runtime_evidence" `
            -Label "runtime evidence bundle" `
            -Role "supporting_evidence" `
            -SummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" `
            -SummaryPath $resolvedRuntimeEvidenceSurface.SummaryPath `
            -ReportMarkdownPath $resolvedRuntimeEvidenceSurface.ReportMarkdownPath `
            -CheckTextPath $resolvedRuntimeEvidenceSurface.CheckTextPath)
    ) | Out-Null
}

$resolvedKernelRuntimeSessionSurface = Resolve-KernelRuntimeSessionSurface `
    -RuntimeEvidenceSummaryPath $RuntimeEvidenceSummary
if ($null -ne $resolvedKernelRuntimeSessionSurface) {
    $supportingSurfaces.Add(
        (New-FrontPageSurface `
            -Id "kernel_runtime_session" `
            -Label "kernel runtime session" `
            -Role "supporting_evidence" `
            -SummarySchema "minimal_kernel.kernel_runtime_session/v0" `
            -SummaryPath $resolvedKernelRuntimeSessionSurface.SummaryPath `
            -ReportMarkdownPath $resolvedKernelRuntimeSessionSurface.ReportMarkdownPath `
            -CheckTextPath $resolvedKernelRuntimeSessionSurface.CheckTextPath)
    ) | Out-Null
}

$updatedFrontPage = [ordered]@{
    summary_path = $resolvedFrontPageSummaryPath
    report_markdown_path = $resolvedFrontPageReportMarkdownPath
    check_text_path = $resolvedFrontPageCheckTextPath
    supporting_surfaces = @($supportingSurfaces)
}

Set-ObjectProperty -Object $rootSummary -Name "front_page" -Value $updatedFrontPage

Ensure-ParentDirectory -Path $resolvedRootSummaryPath
$rootSummary | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $resolvedRootSummaryPath -Encoding utf8

$surfaceIds = @(
    @($supportingSurfaces) |
        ForEach-Object { [string](Get-ObjectPropertyValue -Object $_ -Name "id") } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)

Write-Host ("[WITNESS-FRONT-PAGE] summary={0}" -f $resolvedRootSummaryPath)
Write-Host ("[WITNESS-FRONT-PAGE] supporting_surfaces={0}" -f ($surfaceIds -join ","))
