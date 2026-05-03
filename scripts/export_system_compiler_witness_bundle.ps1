param(
    [string]$CanonicalWorld = "",
    [string]$ArtifactRoot = "",
    [string[]]$ArtifactReport = @(),
    [string[]]$Case = @(),
    [string]$RuntimeEvidenceSummary = "",
    [string]$OutputRoot = "out/system-compiler-witness-bundle",
    [string]$OutputPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = ""
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

function Resolve-RelativeToBase {
    param(
        [string]$BasePath,
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return Resolve-FullPath -Path $Value
    }

    $baseDirectory = Split-Path -Parent $BasePath
    return [System.IO.Path]::GetFullPath((Join-Path $baseDirectory $Value))
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
}

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path $OutputRootPath $DefaultFileName
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Add-CountMapEntry {
    param(
        [hashtable]$Counts,
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return
    }

    if ($Counts.ContainsKey($Name)) {
        $Counts[$Name] = [int]$Counts[$Name] + 1
    } else {
        $Counts[$Name] = 1
    }
}

function ConvertTo-OrderedCountMap {
    param(
        [hashtable]$Counts
    )

    $result = [ordered]@{}
    foreach ($entry in @($Counts.GetEnumerator() | Sort-Object Key)) {
        $result[[string]$entry.Key] = [int]$entry.Value
    }

    return $result
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

function New-WitnessBundleFrontPage {
    param(
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath,
        $RuntimeEvidenceSummaryInfo
    )

    $supportingSurfaces = [System.Collections.Generic.List[object]]::new()
    if ($null -ne $RuntimeEvidenceSummaryInfo) {
        $supportingSurfaces.Add(
            (New-FrontPageSurface `
                -Id "runtime_evidence" `
                -Label "runtime evidence bundle" `
                -Role "supporting_evidence" `
                -SummarySchema ([string]$RuntimeEvidenceSummaryInfo.Data.schema) `
                -SummaryPath $RuntimeEvidenceSummaryInfo.Path `
                -ReportMarkdownPath (Resolve-RelativeToBase -BasePath $RuntimeEvidenceSummaryInfo.Path -Value ([string]$RuntimeEvidenceSummaryInfo.Data.report_markdown_path)) `
                -CheckTextPath (Resolve-RelativeToBase -BasePath $RuntimeEvidenceSummaryInfo.Path -Value ([string]$RuntimeEvidenceSummaryInfo.Data.check_text_path))
            )
        ) | Out-Null
    }

    return [ordered]@{
        summary_path = $SummaryPath
        report_markdown_path = $ReportMarkdownPath
        check_text_path = $CheckTextPath
        supporting_surfaces = @($supportingSurfaces)
    }
}

function New-EntrySubject {
    param(
        [AllowNull()]
        [string]$Case,
        [AllowNull()]
        [string]$Profile,
        [AllowNull()]
        [string]$Board,
        [string[]]$ActiveFacets
    )

    return [ordered]@{
        case = $(if ([string]::IsNullOrWhiteSpace($Case)) { $null } else { [string]$Case })
        profile = $(if ([string]::IsNullOrWhiteSpace($Profile)) { $null } else { [string]$Profile })
        board = $(if ([string]::IsNullOrWhiteSpace($Board)) { $null } else { [string]$Board })
        active_facets = @(
            @($ActiveFacets) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Select-Object -Unique
        )
    }
}

function New-WorldSubject {
    param(
        [AllowNull()]
        [string]$Profile,
        [AllowNull()]
        [string]$Board,
        [string[]]$ActiveFacets
    )

    return [ordered]@{
        profile = $(if ([string]::IsNullOrWhiteSpace($Profile)) { $null } else { [string]$Profile })
        board = $(if ([string]::IsNullOrWhiteSpace($Board)) { $null } else { [string]$Board })
        active_facets = @(
            @($ActiveFacets) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Select-Object -Unique
        )
    }
}

function New-WorldObject {
    return [ordered]@{
        name = "ad_hoc_witness_world"
        title = "Ad Hoc Witness World"
        summary = "A synthesized witness world composed from the currently selected inputs."
        subject = New-WorldSubject -Profile $null -Board $null -ActiveFacets @()
        first_class_terms = @()
        core_questions = @()
        compare_questions = @()
        contract_refs = @()
        witness_plan = @()
    }
}

function Load-CanonicalWorldInfo {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (-not (Test-Path $resolvedPath)) {
        throw "canonical world not found: $resolvedPath"
    }

    $data = Load-JsonObject -Path $resolvedPath
    if ([string]$data.schema -ne "system_compiler.canonical_world/v0") {
        throw "unsupported canonical world schema: $([string]$data.schema)"
    }
    if ([string]$data.kind -ne "system_compiler.canonical_world") {
        throw "unsupported canonical world kind: $([string]$data.kind)"
    }

    return [pscustomobject]@{
        Path = $resolvedPath
        Data = $data
    }
}

function Load-ArtifactReportInfo {
    param(
        [string]$Path
    )

    $resolvedPath = Resolve-FullPath -Path $Path
    if (-not (Test-Path $resolvedPath)) {
        throw "artifact report not found: $resolvedPath"
    }

    $data = Load-JsonObject -Path $resolvedPath
    if ([string]$data.schema -ne "system_compiler.artifact_report/v0") {
        throw "unsupported artifact report schema: $([string]$data.schema)"
    }

    return [pscustomobject]@{
        Path = $resolvedPath
        Data = $data
    }
}

function Get-ArtifactReportInfos {
    $reportPaths = @()
    if (@($ArtifactReport).Count -gt 0) {
        $reportPaths = @($ArtifactReport)
    } elseif (-not [string]::IsNullOrWhiteSpace($ArtifactRoot)) {
        $resolvedArtifactRoot = Resolve-FullPath -Path $ArtifactRoot
        if (-not (Test-Path $resolvedArtifactRoot)) {
            throw "artifact root not found: $resolvedArtifactRoot"
        }

        $candidateFiles = @(
            Get-ChildItem -LiteralPath $resolvedArtifactRoot -Filter "*.artifact_report.json" -File |
                Sort-Object Name
        )

        if (@($Case).Count -eq 0) {
            $reportPaths = @($candidateFiles | ForEach-Object { $_.FullName })
        } else {
            foreach ($caseName in @($Case)) {
                $matched = @(
                    @($candidateFiles) |
                        Where-Object {
                            [System.IO.Path]::GetFileNameWithoutExtension(
                                [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
                            ) -eq [string]$caseName
                        } |
                        Select-Object -First 1
                )
                if ($matched.Count -eq 0) {
                    throw "artifact report case not found under artifact root: $caseName"
                }
                $reportPaths += $matched[0].FullName
            }
        }
    }

    $infos = @()
    foreach ($reportPath in @($reportPaths)) {
        $infos += Load-ArtifactReportInfo -Path $reportPath
    }
    return @($infos)
}

function Get-ArtifactReportIndexPath {
    param(
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($RootPath)) {
        return $null
    }

    $resolvedArtifactRoot = Resolve-FullPath -Path $RootPath
    $candidatePath = Join-Path $resolvedArtifactRoot "index.json"
    if (-not (Test-Path -LiteralPath $candidatePath)) {
        return $null
    }

    $data = Load-JsonObject -Path $candidatePath
    if ([string]$data.schema -ne "system_compiler.artifact_report_index/v0") {
        throw "unsupported artifact report index schema: $([string]$data.schema)"
    }

    return (Resolve-FullPath -Path $candidatePath)
}

function Load-RuntimeEvidenceSummaryInfo {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (-not (Test-Path $resolvedPath)) {
        throw "runtime evidence summary not found: $resolvedPath"
    }

    $data = Load-JsonObject -Path $resolvedPath
    if ([string]$data.schema -ne "minimal_kernel.runtime_evidence_bundle.summary/v1") {
        throw "unsupported runtime evidence summary schema: $([string]$data.schema)"
    }

    return [pscustomobject]@{
        Path = $resolvedPath
        Data = $data
    }
}

function Get-ResolvedContractRefs {
    param(
        $CanonicalWorldInfo
    )

    if ($null -eq $CanonicalWorldInfo) {
        return @()
    }

    return @(
        @($CanonicalWorldInfo.Data.contract_refs) |
            ForEach-Object { Resolve-RelativeToBase -BasePath $CanonicalWorldInfo.Path -Value ([string]$_) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Select-Object -Unique
    )
}

function Get-ExistingArtifactRefsFromReport {
    param(
        $ReportInfo
    )

    $refs = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $ReportInfo) {
        return @()
    }

    $refs.Add($ReportInfo.Path) | Out-Null
    if ($null -eq $ReportInfo.Data.PSObject.Properties["artifacts"] -or $null -eq $ReportInfo.Data.artifacts) {
        return @($refs | Select-Object -Unique)
    }

    foreach ($property in @($ReportInfo.Data.artifacts.PSObject.Properties)) {
        $value = [string]$property.Value
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }

        if ([System.IO.Path]::IsPathRooted($value)) {
            $candidate = Resolve-FullPath -Path $value
        } else {
            $candidate = Resolve-FullPath -Path $value
        }

        if (Test-Path $candidate) {
            $refs.Add($candidate) | Out-Null
        }
    }

    return @($refs | Select-Object -Unique)
}

function Get-ExistingArtifactRefsFromRuntimeEvidenceSummary {
    param(
        $SummaryInfo
    )

    $refs = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $SummaryInfo) {
        return @()
    }

    $refs.Add($SummaryInfo.Path) | Out-Null
    return @($refs | Select-Object -Unique)
}

function New-MissingWitnessEntry {
    param(
        $PlanEntry,
        [string]$SourcePath,
        [string]$Observation
    )

    return [ordered]@{
        id = [string]$PlanEntry.id
        kind = [string]$PlanEntry.kind
        label = [string]$PlanEntry.label
        role = [string]$PlanEntry.role
        layer = [string]$PlanEntry.layer
        required = [bool]$PlanEntry.required
        status = "missing"
        witness_focus = @($PlanEntry.witness_focus)
        case = $(if ([string]::IsNullOrWhiteSpace([string]$PlanEntry.case)) { $null } else { [string]$PlanEntry.case })
        source_path = $(if ([string]::IsNullOrWhiteSpace($SourcePath)) { $null } else { $SourcePath })
        subject = New-EntrySubject -Case $null -Profile $null -Board $null -ActiveFacets @()
        observations = @($Observation)
        artifact_refs = @()
    }
}

function New-ArtifactReportWitnessEntry {
    param(
        $PlanEntry,
        $ReportInfo
    )

    $report = $ReportInfo.Data
    $observations = [System.Collections.Generic.List[string]]::new()
    $observations.Add(("mode={0}" -f [string]$report.mode)) | Out-Null
    if ($null -ne $report.PSObject.Properties["system_formation"] -and $null -ne $report.system_formation) {
        $observations.Add(("formation={0}" -f [string]$report.system_formation.status)) | Out-Null
    }
    $observations.Add(("bindings=resolved:{0} unresolved:{1}" -f [int]$report.binding_result.resolved_binding_count, [int]$report.binding_result.unresolved_binding_count)) | Out-Null
    $observations.Add(("bringup=published:{0} observed:{1} blocked:{2} failed:{3}" -f [int]$report.bringup_evidence.published_count, [int]$report.bringup_evidence.observed_count, [int]$report.bringup_evidence.blocked_count, [int]$report.bringup_evidence.failed_count)) | Out-Null
    if ($null -ne $report.PSObject.Properties["comparison"] -and $null -ne $report.comparison) {
        $observations.Add(("compare={0}" -f [string]$report.comparison.status)) | Out-Null
    }

    return [ordered]@{
        id = [string]$PlanEntry.id
        kind = "artifact_report"
        label = [string]$PlanEntry.label
        role = [string]$PlanEntry.role
        layer = [string]$PlanEntry.layer
        required = [bool]$PlanEntry.required
        status = "ok"
        witness_focus = @($PlanEntry.witness_focus)
        case = [string]$report.subject.case
        source_path = $ReportInfo.Path
        subject = New-EntrySubject -Case ([string]$report.subject.case) -Profile ([string]$report.subject.profile) -Board ([string]$report.subject.board) -ActiveFacets @($report.subject.active_facets)
        observations = @($observations)
        artifact_refs = @(Get-ExistingArtifactRefsFromReport -ReportInfo $ReportInfo)
    }
}

function New-RuntimeEvidenceWitnessEntry {
    param(
        $PlanEntry,
        $SummaryInfo
    )

    if ($null -eq $SummaryInfo) {
        return (New-MissingWitnessEntry -PlanEntry $PlanEntry -SourcePath $null -Observation "runtime evidence summary missing")
    }

    $summary = $SummaryInfo.Data
    $observations = [System.Collections.Generic.List[string]]::new()
    $observations.Add(("result={0}" -f [string]$summary.result)) | Out-Null

    if ($null -ne $summary.host -and $null -ne $summary.host.cold) {
        $observations.Add(("host_cold=ok:{0} fail:{1} other:{2}" -f [int]$summary.host.cold.status.ok, [int]$summary.host.cold.status.fail, [int]$summary.host.cold.status.other)) | Out-Null
    }
    if ($null -ne $summary.host -and $null -ne $summary.host.warm) {
        $observations.Add(("host_warm=ok:{0} fail:{1} other:{2}" -f [int]$summary.host.warm.status.ok, [int]$summary.host.warm.status.fail, [int]$summary.host.warm.status.other)) | Out-Null
    }
    if ($null -ne $summary.qemu -and $null -ne $summary.qemu.lower_half) {
        $observations.Add(("qemu=ok:{0} fail:{1} other:{2}" -f [int]$summary.qemu.lower_half.status.ok, [int]$summary.qemu.lower_half.status.fail, [int]$summary.qemu.lower_half.status.other)) | Out-Null
    }

    $runtimeCaseName = if ([string]::IsNullOrWhiteSpace([string]$PlanEntry.case)) {
        [string]$PlanEntry.label
    } else {
        [string]$PlanEntry.case
    }

    return [ordered]@{
        id = [string]$PlanEntry.id
        kind = "runtime_evidence_bundle"
        label = [string]$PlanEntry.label
        role = [string]$PlanEntry.role
        layer = [string]$PlanEntry.layer
        required = [bool]$PlanEntry.required
        status = $(if ([string]$summary.result -eq "ok") { "ok" } else { "fail" })
        witness_focus = @($PlanEntry.witness_focus)
        case = $runtimeCaseName
        source_path = $SummaryInfo.Path
        subject = New-EntrySubject -Case $runtimeCaseName -Profile $null -Board $null -ActiveFacets @()
        observations = @($observations)
        artifact_refs = @(Get-ExistingArtifactRefsFromRuntimeEvidenceSummary -SummaryInfo $SummaryInfo)
    }
}

function New-ExampleRefWitnessEntry {
    param(
        $PlanEntry,
        [string]$ResolvedPath
    )

    if ([string]::IsNullOrWhiteSpace($ResolvedPath) -or -not (Test-Path $ResolvedPath)) {
        return (New-MissingWitnessEntry -PlanEntry $PlanEntry -SourcePath $ResolvedPath -Observation "example reference missing")
    }

    return [ordered]@{
        id = [string]$PlanEntry.id
        kind = "example_ref"
        label = [string]$PlanEntry.label
        role = [string]$PlanEntry.role
        layer = [string]$PlanEntry.layer
        required = [bool]$PlanEntry.required
        status = "ok"
        witness_focus = @($PlanEntry.witness_focus)
        case = $(if ([string]::IsNullOrWhiteSpace([string]$PlanEntry.case)) { $null } else { [string]$PlanEntry.case })
        source_path = $ResolvedPath
        subject = New-EntrySubject -Case $null -Profile $null -Board $null -ActiveFacets @()
        observations = @(("path={0}" -f $ResolvedPath))
        artifact_refs = @()
    }
}

function New-ResolvedPlanEntry {
    param(
        $Entry,
        $CanonicalWorldInfo
    )

    $resolvedPath = $null
    if (-not [string]::IsNullOrWhiteSpace([string]$Entry.path)) {
        if ($null -ne $CanonicalWorldInfo) {
            $resolvedPath = Resolve-RelativeToBase -BasePath $CanonicalWorldInfo.Path -Value ([string]$Entry.path)
        } else {
            $resolvedPath = Resolve-FullPath -Path ([string]$Entry.path)
        }
    }

    return [ordered]@{
        id = [string]$Entry.id
        kind = [string]$Entry.kind
        label = [string]$Entry.label
        role = [string]$Entry.role
        layer = [string]$Entry.layer
        required = [bool]$Entry.required
        witness_focus = @($Entry.witness_focus)
        case = $(if ([string]::IsNullOrWhiteSpace([string]$Entry.case)) { $null } else { [string]$Entry.case })
        path = $(if ([string]::IsNullOrWhiteSpace($resolvedPath)) { $null } else { $resolvedPath })
    }
}

function New-DiscoveredPlanEntry {
    param(
        [string]$Id,
        [string]$Kind,
        [string]$Label,
        [string]$Role,
        [string]$Layer,
        [string[]]$WitnessFocus,
        [string]$CaseName,
        [string]$Path
    )

    return [ordered]@{
        id = $Id
        kind = $Kind
        label = $Label
        role = $Role
        layer = $Layer
        required = $true
        witness_focus = @($WitnessFocus)
        case = $(if ([string]::IsNullOrWhiteSpace($CaseName)) { $null } else { $CaseName })
        path = $(if ([string]::IsNullOrWhiteSpace($Path)) { $null } else { $Path })
    }
}

function Get-DiscoveredPlan {
    param(
        $ArtifactReportInfos,
        $RuntimeEvidenceSummaryInfo
    )

    $plan = @()
    foreach ($reportInfo in @($ArtifactReportInfos)) {
        $caseName = [string]$reportInfo.Data.subject.case
        $plan += New-DiscoveredPlanEntry `
            -Id ("artifact_report::{0}" -f $caseName) `
            -Kind "artifact_report" `
            -Label $caseName `
            -Role "discovered artifact report witness" `
            -Layer "system_compiler" `
            -WitnessFocus @("artifact", "report") `
            -CaseName $caseName `
            -Path $reportInfo.Path
    }

    if ($null -ne $RuntimeEvidenceSummaryInfo) {
        $plan += New-DiscoveredPlanEntry `
            -Id "runtime_evidence_bundle" `
            -Kind "runtime_evidence_bundle" `
            -Label "minimal-kernel-runtime-evidence" `
            -Role "discovered runtime evidence aggregation witness" `
            -Layer "bundle" `
            -WitnessFocus @("upper-half", "lower-half", "bundle") `
            -CaseName "minimal-kernel-runtime-evidence" `
            -Path $RuntimeEvidenceSummaryInfo.Path
    }

    return @($plan)
}

function New-ResolvedWorld {
    param(
        $CanonicalWorldInfo,
        $ArtifactReportInfos,
        $RuntimeEvidenceSummaryInfo
    )

    if ($null -eq $CanonicalWorldInfo) {
        $world = New-WorldObject
        $world.witness_plan = @(Get-DiscoveredPlan -ArtifactReportInfos $ArtifactReportInfos -RuntimeEvidenceSummaryInfo $RuntimeEvidenceSummaryInfo)
        return $world
    }

    return [ordered]@{
        name = [string]$CanonicalWorldInfo.Data.name
        title = [string]$CanonicalWorldInfo.Data.title
        summary = [string]$CanonicalWorldInfo.Data.summary
        subject = New-WorldSubject -Profile ([string]$CanonicalWorldInfo.Data.subject.profile) -Board ([string]$CanonicalWorldInfo.Data.subject.board) -ActiveFacets @($CanonicalWorldInfo.Data.subject.active_facets)
        first_class_terms = @($CanonicalWorldInfo.Data.first_class_terms)
        core_questions = @($CanonicalWorldInfo.Data.core_questions)
        compare_questions = @($CanonicalWorldInfo.Data.compare_questions)
        contract_refs = @(Get-ResolvedContractRefs -CanonicalWorldInfo $CanonicalWorldInfo)
        witness_plan = @(
            @($CanonicalWorldInfo.Data.witness_plan) |
                ForEach-Object { New-ResolvedPlanEntry -Entry $_ -CanonicalWorldInfo $CanonicalWorldInfo }
        )
    }
}

function Get-ArtifactReportMaps {
    param(
        $ArtifactReportInfos
    )

    $byPath = @{}
    $byCase = @{}
    foreach ($reportInfo in @($ArtifactReportInfos)) {
        $byPath[$reportInfo.Path] = $reportInfo
        $caseName = [string]$reportInfo.Data.subject.case
        if (-not [string]::IsNullOrWhiteSpace($caseName)) {
            $byCase[$caseName] = $reportInfo
        }
    }

    return [pscustomobject]@{
        ByPath = $byPath
        ByCase = $byCase
    }
}

function Resolve-ArtifactReportForPlanEntry {
    param(
        $PlanEntry,
        $ArtifactReportMaps
    )

    if ($null -ne $PlanEntry.path -and $ArtifactReportMaps.ByPath.ContainsKey([string]$PlanEntry.path)) {
        return $ArtifactReportMaps.ByPath[[string]$PlanEntry.path]
    }

    if ($null -ne $PlanEntry.path -and (Test-Path ([string]$PlanEntry.path))) {
        return Load-ArtifactReportInfo -Path ([string]$PlanEntry.path)
    }

    if ($null -ne $PlanEntry.case -and $ArtifactReportMaps.ByCase.ContainsKey([string]$PlanEntry.case)) {
        return $ArtifactReportMaps.ByCase[[string]$PlanEntry.case]
    }

    return $null
}

function Resolve-RuntimeEvidenceForPlanEntry {
    param(
        $PlanEntry,
        $RuntimeEvidenceSummaryInfo
    )

    if ($null -ne $RuntimeEvidenceSummaryInfo) {
        return $RuntimeEvidenceSummaryInfo
    }

    if ($null -ne $PlanEntry.path -and (Test-Path ([string]$PlanEntry.path))) {
        return Load-RuntimeEvidenceSummaryInfo -Path ([string]$PlanEntry.path)
    }

    return $null
}

function New-WitnessEntry {
    param(
        $PlanEntry,
        $ArtifactReportMaps,
        $RuntimeEvidenceSummaryInfo
    )

    switch ([string]$PlanEntry.kind) {
        "artifact_report" {
            $reportInfo = Resolve-ArtifactReportForPlanEntry -PlanEntry $PlanEntry -ArtifactReportMaps $ArtifactReportMaps
            if ($null -eq $reportInfo) {
                return (New-MissingWitnessEntry -PlanEntry $PlanEntry -SourcePath ([string]$PlanEntry.path) -Observation "artifact report missing")
            }
            return (New-ArtifactReportWitnessEntry -PlanEntry $PlanEntry -ReportInfo $reportInfo)
        }
        "runtime_evidence_bundle" {
            $summaryInfo = Resolve-RuntimeEvidenceForPlanEntry -PlanEntry $PlanEntry -RuntimeEvidenceSummaryInfo $RuntimeEvidenceSummaryInfo
            return (New-RuntimeEvidenceWitnessEntry -PlanEntry $PlanEntry -SummaryInfo $summaryInfo)
        }
        "example_ref" {
            return (New-ExampleRefWitnessEntry -PlanEntry $PlanEntry -ResolvedPath ([string]$PlanEntry.path))
        }
        default {
            throw "unsupported witness plan kind: $([string]$PlanEntry.kind)"
        }
    }
}

$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/system-compiler-witness-bundle" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $OutputPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"

$canonicalWorldInfo = Load-CanonicalWorldInfo -Path $CanonicalWorld
$artifactReportInfos = @(Get-ArtifactReportInfos)
$artifactReportIndexPath = Get-ArtifactReportIndexPath -RootPath $ArtifactRoot
$runtimeEvidenceSummaryInfo = Load-RuntimeEvidenceSummaryInfo -Path $RuntimeEvidenceSummary
$resolvedWorld = New-ResolvedWorld -CanonicalWorldInfo $canonicalWorldInfo -ArtifactReportInfos $artifactReportInfos -RuntimeEvidenceSummaryInfo $runtimeEvidenceSummaryInfo
$artifactReportMaps = Get-ArtifactReportMaps -ArtifactReportInfos $artifactReportInfos

$contractPresentRefs = @()
$contractMissingRefs = @()
foreach ($contractRef in @($resolvedWorld.contract_refs)) {
    if (Test-Path $contractRef) {
        $contractPresentRefs += $contractRef
    } else {
        $contractMissingRefs += $contractRef
    }
}

$witnessEntries = @()
foreach ($planEntry in @($resolvedWorld.witness_plan)) {
    $witnessEntries += New-WitnessEntry -PlanEntry $planEntry -ArtifactReportMaps $artifactReportMaps -RuntimeEvidenceSummaryInfo $runtimeEvidenceSummaryInfo
}

$kindCounts = @{}
$okCount = 0
$missingCount = 0
$failCount = 0
$requiredMissingCount = 0
$violations = [System.Collections.Generic.List[string]]::new()

foreach ($entry in @($witnessEntries)) {
    Add-CountMapEntry -Counts $kindCounts -Name ([string]$entry.kind)

    switch ([string]$entry.status) {
        "ok" {
            $okCount++
        }
        "missing" {
            $missingCount++
            if ([bool]$entry.required) {
                $requiredMissingCount++
                $violations.Add(("missing required witness: {0}" -f [string]$entry.id)) | Out-Null
            }
        }
        "fail" {
            $failCount++
            $violations.Add(("failed witness: {0}" -f [string]$entry.id)) | Out-Null
        }
    }
}

foreach ($missingContractRef in @($contractMissingRefs)) {
    $violations.Add(("missing contract ref: {0}" -f $missingContractRef)) | Out-Null
}

if (@($witnessEntries).Count -eq 0) {
    $violations.Add("witness bundle contains no witness entries") | Out-Null
}

$bundleObject = [ordered]@{
    schema = "system_compiler.witness_bundle/v0"
    kind = "system_compiler.witness_bundle"
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    generator = "scripts/export_system_compiler_witness_bundle.ps1"
    result = if ($violations.Count -eq 0) { "ok" } else { "fail" }
    world = $resolvedWorld
    front_page = New-WitnessBundleFrontPage `
        -SummaryPath $summaryPathResolved `
        -ReportMarkdownPath $reportMarkdownPathResolved `
        -CheckTextPath $checkTextPathResolved `
        -RuntimeEvidenceSummaryInfo $runtimeEvidenceSummaryInfo
    artifact_context = [ordered]@{
        canonical_world = if ($null -eq $canonicalWorldInfo) { $null } else { $canonicalWorldInfo.Path }
        artifact_root = if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) { $null } else { Resolve-FullPath -Path $ArtifactRoot }
        artifact_report_index = $artifactReportIndexPath
        artifact_reports = @($artifactReportInfos | ForEach-Object { $_.Path })
        runtime_evidence_summary = if ($null -eq $runtimeEvidenceSummaryInfo) { $null } else { $runtimeEvidenceSummaryInfo.Path }
        output_root = $outputRootPath
        report_markdown_path = $reportMarkdownPathResolved
        check_text_path = $checkTextPathResolved
    }
    contract_status = [ordered]@{
        declared_count = @($resolvedWorld.contract_refs).Count
        present_count = @($contractPresentRefs).Count
        missing_count = @($contractMissingRefs).Count
        present_refs = @($contractPresentRefs)
        missing_refs = @($contractMissingRefs)
    }
    witness_summary = [ordered]@{
        entry_count = @($witnessEntries).Count
        ok_count = $okCount
        missing_count = $missingCount
        fail_count = $failCount
        required_missing_count = $requiredMissingCount
        kind_counts = ConvertTo-OrderedCountMap -Counts $kindCounts
    }
    witness_entries = @($witnessEntries)
    violations = @($violations)
}

Ensure-ParentDirectory -Path $summaryPathResolved
$bundleObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8

$reportBuilder = [System.Text.StringBuilder]::new()
[void]$reportBuilder.AppendLine("# System Compiler Witness Bundle")
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine(("- Generated at: {0}" -f [string]$bundleObject.generated_at_utc))
[void]$reportBuilder.AppendLine(("- Result: {0}" -f [string]$bundleObject.result))
[void]$reportBuilder.AppendLine(("- Summary JSON: {0}" -f $summaryPathResolved))
[void]$reportBuilder.AppendLine(("- World: {0}" -f [string]$bundleObject.world.name))
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## World")
[void]$reportBuilder.AppendLine(("- Title: {0}" -f [string]$bundleObject.world.title))
[void]$reportBuilder.AppendLine(("- Summary: {0}" -f [string]$bundleObject.world.summary))
if (-not [string]::IsNullOrWhiteSpace([string]$bundleObject.world.subject.profile)) {
    [void]$reportBuilder.AppendLine(("- Profile: {0}" -f [string]$bundleObject.world.subject.profile))
}
if (-not [string]::IsNullOrWhiteSpace([string]$bundleObject.world.subject.board)) {
    [void]$reportBuilder.AppendLine(("- Board: {0}" -f [string]$bundleObject.world.subject.board))
}
if (@($bundleObject.world.subject.active_facets).Count -gt 0) {
    [void]$reportBuilder.AppendLine(("- Active facets: {0}" -f ((@($bundleObject.world.subject.active_facets) -join ", "))))
}
if (@($bundleObject.world.first_class_terms).Count -gt 0) {
    [void]$reportBuilder.AppendLine(("- First-class terms: {0}" -f ((@($bundleObject.world.first_class_terms) -join ", "))))
}

if (@($bundleObject.world.core_questions).Count -gt 0) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Core Questions")
    foreach ($question in @($bundleObject.world.core_questions)) {
        [void]$reportBuilder.AppendLine(("- {0}" -f [string]$question))
    }
}

if (@($bundleObject.world.compare_questions).Count -gt 0) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Compare Questions")
    foreach ($question in @($bundleObject.world.compare_questions)) {
        [void]$reportBuilder.AppendLine(("- {0}" -f [string]$question))
    }
}

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Contract Status")
[void]$reportBuilder.AppendLine(("- Declared refs: {0}" -f [int]$bundleObject.contract_status.declared_count))
[void]$reportBuilder.AppendLine(("- Present refs: {0}" -f [int]$bundleObject.contract_status.present_count))
[void]$reportBuilder.AppendLine(("- Missing refs: {0}" -f [int]$bundleObject.contract_status.missing_count))

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Witness Summary")
[void]$reportBuilder.AppendLine(("- Entries: {0}" -f [int]$bundleObject.witness_summary.entry_count))
[void]$reportBuilder.AppendLine(("- OK: {0}" -f [int]$bundleObject.witness_summary.ok_count))
[void]$reportBuilder.AppendLine(("- Missing: {0}" -f [int]$bundleObject.witness_summary.missing_count))
[void]$reportBuilder.AppendLine(("- Fail: {0}" -f [int]$bundleObject.witness_summary.fail_count))
[void]$reportBuilder.AppendLine(("- Required missing: {0}" -f [int]$bundleObject.witness_summary.required_missing_count))

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Witness Entries")
foreach ($entry in @($bundleObject.witness_entries)) {
    [void]$reportBuilder.AppendLine(("- {0} {1} {2}" -f [string]$entry.id, [string]$entry.kind, [string]$entry.status))
    [void]$reportBuilder.AppendLine(("  role={0} layer={1}" -f [string]$entry.role, [string]$entry.layer))
    if (-not [string]::IsNullOrWhiteSpace([string]$entry.source_path)) {
        [void]$reportBuilder.AppendLine(("  source={0}" -f [string]$entry.source_path))
    }
    if (@($entry.witness_focus).Count -gt 0) {
        [void]$reportBuilder.AppendLine(("  focus={0}" -f ((@($entry.witness_focus) -join ", "))))
    }
    foreach ($observation in @($entry.observations)) {
        [void]$reportBuilder.AppendLine(("  note={0}" -f [string]$observation))
    }
}

if ($violations.Count -gt 0) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Violations")
    foreach ($message in $violations) {
        [void]$reportBuilder.AppendLine(("- {0}" -f [string]$message))
    }
}

Ensure-ParentDirectory -Path $reportMarkdownPathResolved
Set-Content -LiteralPath $reportMarkdownPathResolved -Encoding utf8 ($reportBuilder.ToString())

$checkBuilder = [System.Text.StringBuilder]::new()
[void]$checkBuilder.AppendLine(("summary: {0}" -f $summaryPathResolved))
[void]$checkBuilder.AppendLine(("result: {0}" -f [string]$bundleObject.result))
[void]$checkBuilder.AppendLine(("world: {0}" -f [string]$bundleObject.world.name))
[void]$checkBuilder.AppendLine(("entries: {0}" -f [int]$bundleObject.witness_summary.entry_count))
[void]$checkBuilder.AppendLine(("required_missing: {0}" -f [int]$bundleObject.witness_summary.required_missing_count))
if ($violations.Count -gt 0) {
    [void]$checkBuilder.AppendLine("violations:")
    foreach ($message in $violations) {
        [void]$checkBuilder.AppendLine(("- {0}" -f [string]$message))
    }
}
Set-Content -LiteralPath $checkTextPathResolved -Encoding utf8 ($checkBuilder.ToString())

Write-Host "[WITNESS] summary=$summaryPathResolved"
Write-Host "[WITNESS] report=$reportMarkdownPathResolved"
Write-Host "[WITNESS] check=$checkTextPathResolved"

if ($violations.Count -gt 0) {
    throw "witness bundle contains violations"
}
