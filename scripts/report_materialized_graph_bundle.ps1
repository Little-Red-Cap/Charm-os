param(
    [string]$LeftBundleRoot = "",
    [string]$RightBundleRoot = "",
    [string]$LeftIndex = "",
    [string]$RightIndex = "",
    [string[]]$Case = @(),
    [switch]$IncludeUnchanged,
    [ValidateSet('markdown', 'html', 'both')]
    [string]$Format = 'both',
    [string]$OutputDir = 'out/materialized-graph-report',
    [string]$MarkdownPath = "",
    [string]$HtmlPath = "",
    [string]$ManifestPath = "",
    [string]$Title = 'Materialized Graph Bundle Diff Report'
)

$ErrorActionPreference = 'Stop'

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ''
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
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseFull = Resolve-FullPath $BasePath
    $targetFull = Resolve-FullPath $TargetPath
    $trimChars = [char[]]@('\', '/')
    $baseUri = New-Object System.Uri(($baseFull.TrimEnd($trimChars) + [System.IO.Path]::DirectorySeparatorChar))
    $targetUri = New-Object System.Uri($targetFull)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Get-DiffData {
    $diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
    if (-not (Test-Path $diffScript)) {
        throw "diff script not found: $diffScript"
    }

    $diffArgs = @{ AsJson = $true }
    if (-not [string]::IsNullOrWhiteSpace($LeftBundleRoot)) {
        $diffArgs.LeftBundleRoot = $LeftBundleRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($RightBundleRoot)) {
        $diffArgs.RightBundleRoot = $RightBundleRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($LeftIndex)) {
        $diffArgs.LeftIndex = $LeftIndex
    }
    if (-not [string]::IsNullOrWhiteSpace($RightIndex)) {
        $diffArgs.RightIndex = $RightIndex
    }
    if ($Case.Count -gt 0) {
        $diffArgs.Case = $Case
    }
    if ($IncludeUnchanged) {
        $diffArgs.IncludeUnchanged = $true
    }

    $jsonText = (& $diffScript @diffArgs | Out-String)
    return ($jsonText | ConvertFrom-Json)
}

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath $ExplicitPath
    }

    $outputRoot = Resolve-FullPath $OutputDir
    if (-not (Test-Path $outputRoot)) {
        New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
    }

    return Join-Path $outputRoot $DefaultFileName
}

function Get-ManifestOutputPath {
    if (-not [string]::IsNullOrWhiteSpace($ManifestPath)) {
        return Resolve-FullPath $ManifestPath
    }

    if (-not [string]::IsNullOrWhiteSpace($MarkdownPath)) {
        return Join-Path (Split-Path -Parent (Resolve-FullPath $MarkdownPath)) 'materialized_graph_bundle_diff_report.manifest.json'
    }

    if (-not [string]::IsNullOrWhiteSpace($HtmlPath)) {
        return Join-Path (Split-Path -Parent (Resolve-FullPath $HtmlPath)) 'materialized_graph_bundle_diff_report.manifest.json'
    }

    return Get-OutputPath -ExplicitPath '' -DefaultFileName 'materialized_graph_bundle_diff_report.manifest.json'
}

function Get-CaseStatusCount {
    param(
        $Cases,
        [string]$Status
    )

    return @($Cases | Where-Object { $_.status -eq $Status }).Count
}

function New-ReportManifest {
    param(
        $DiffData,
        [string]$ResolvedMarkdownPath,
        [string]$ResolvedHtmlPath,
        [string]$ResolvedManifestPath
    )

    $cases = @($DiffData.cases)
    return [ordered]@{
        schema = 'materialized_graph.report_manifest/v1'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        title = $Title
        format = $Format
        include_unchanged = $IncludeUnchanged.IsPresent
        left = [ordered]@{
            index = [string]$DiffData.left.index
            bundle_root = [string]$DiffData.left.bundle_root
            input_manifest = if ($null -ne $DiffData.left.PSObject.Properties['input_manifest']) { $DiffData.left.input_manifest } else { $null }
            declared_facts_defaults = if ($null -ne $DiffData.left.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.left.declared_facts_defaults) { ,@($DiffData.left.declared_facts_defaults) } else { $null }
            declared_contracts_defaults = if ($null -ne $DiffData.left.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.left.declared_contracts_defaults) { ,@($DiffData.left.declared_contracts_defaults) } else { $null }
        }
        right = [ordered]@{
            index = [string]$DiffData.right.index
            bundle_root = [string]$DiffData.right.bundle_root
            input_manifest = if ($null -ne $DiffData.right.PSObject.Properties['input_manifest']) { $DiffData.right.input_manifest } else { $null }
            declared_facts_defaults = if ($null -ne $DiffData.right.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.right.declared_facts_defaults) { ,@($DiffData.right.declared_facts_defaults) } else { $null }
            declared_contracts_defaults = if ($null -ne $DiffData.right.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.right.declared_contracts_defaults) { ,@($DiffData.right.declared_contracts_defaults) } else { $null }
        }
        diff = [ordered]@{
            schema = if ($null -ne $DiffData.PSObject.Properties['schema']) { [string]$DiffData.schema } else { $null }
            case_count = [int]$DiffData.case_count
            status_counts = [ordered]@{
                changed = Get-CaseStatusCount -Cases $cases -Status 'changed'
                added = Get-CaseStatusCount -Cases $cases -Status 'added'
                removed = Get-CaseStatusCount -Cases $cases -Status 'removed'
                unchanged = Get-CaseStatusCount -Cases $cases -Status 'unchanged'
            }
        }
        reports = [ordered]@{
            manifest = $ResolvedManifestPath
            markdown = if (-not [string]::IsNullOrWhiteSpace($ResolvedMarkdownPath) -and (Test-Path $ResolvedMarkdownPath)) { $ResolvedMarkdownPath } else { $null }
            html = if (-not [string]::IsNullOrWhiteSpace($ResolvedHtmlPath) -and (Test-Path $ResolvedHtmlPath)) { $ResolvedHtmlPath } else { $null }
        }
        cases = @($cases | ForEach-Object {
            [ordered]@{
                name = [string]$_.name
                status = [string]$_.status
                summary_changes = @($_.summary_changes)
                metadata_changes = if ($null -ne $_.PSObject.Properties['metadata_changes']) { ,@($_.metadata_changes) } else { @() }
            }
        })
    }
}

function Escape-MarkdownCell {
    param(
        [string]$Text
    )

    if ($null -eq $Text) {
        return ''
    }

    $escaped = $Text.Replace('|', '\|').Replace('`', '\`').Replace("`r", '')
    return $escaped.Replace("`n", '<br/>')
}

function Escape-HtmlText {
    param(
        [string]$Text
    )

    if ($null -eq $Text) {
        return ''
    }

    return [System.Net.WebUtility]::HtmlEncode($Text)
}

function Join-Strings {
    param(
        [object[]]$Items,
        [string]$Separator = ', '
    )

    return (@($Items) -join $Separator)
}

function Format-DeclaredContracts {
    param(
        $Contracts
    )

    return @(
        @($Contracts) |
            ForEach-Object {
                $contractName = [string]$_.contract
                $requires = if ($null -ne $_ -and $null -ne $_.PSObject.Properties['requires']) { @($_.requires) } else { @() }
                if ([string]::IsNullOrWhiteSpace($contractName)) {
                    $null
                } else {
                    "$contractName requires [$((@($requires) -join ', '))]"
                }
            } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
    )
}

function Get-CaseStringProperty {
    param(
        $CaseEntry,
        [string]$PropertyName
    )

    if ($null -eq $CaseEntry -or [string]::IsNullOrWhiteSpace($PropertyName)) {
        return $null
    }

    $property = $CaseEntry.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    $value = [string]$property.Value
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }

    return $value
}

function Get-CaseKind {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return 'absent'
    }

    $caseKind = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'case_kind'
    if (-not [string]::IsNullOrWhiteSpace($caseKind)) {
        return $caseKind
    }

    if ($null -ne (Get-CaseGraphSummary -CaseEntry $CaseEntry)) {
        return 'materialized_graph'
    }

    if (-not [string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'runtime_observe'))) {
        return 'runtime_only'
    }

    return 'materialized_graph'
}

function Get-CaseGraphSummary {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['graph']) {
        return $null
    }

    return $CaseEntry.graph
}

function Get-CaseNodeCount {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return 0
    }

    return [int]$graphSummary.node_count
}

function Get-CaseEdgeCount {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return 0
    }

    return [int]$graphSummary.edge_count
}

function Get-ArtifactLinkMarkdown {
    param(
        [string]$ReportPath,
        $CaseEntry,
        [string]$FieldName,
        [string]$Label
    )

    if ($null -eq $CaseEntry) {
        return ''
    }

    $targetPath = [string]$CaseEntry.$FieldName
    if ([string]::IsNullOrWhiteSpace($targetPath)) {
        return ''
    }

    $relativePath = Get-RelativePath -BasePath (Split-Path -Parent $ReportPath) -TargetPath $targetPath
    return "[$Label]($relativePath)"
}

function Get-ArtifactLinkHtml {
    param(
        [string]$ReportPath,
        $CaseEntry,
        [string]$FieldName,
        [string]$Label
    )

    if ($null -eq $CaseEntry) {
        return ''
    }

    $targetPath = [string]$CaseEntry.$FieldName
    if ([string]::IsNullOrWhiteSpace($targetPath)) {
        return ''
    }

    $relativePath = Get-RelativePath -BasePath (Split-Path -Parent $ReportPath) -TargetPath $targetPath
    return '<a href=' + (Escape-HtmlText $relativePath) + '>' + (Escape-HtmlText $Label) + '</a>'
}

function Add-MarkdownTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string[]]$Headers,
        [object[]]$Rows
    )

    if ($Headers.Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine('| ' + (($Headers | ForEach-Object { Escape-MarkdownCell $_ }) -join ' | ') + ' |')
    [void]$Builder.AppendLine('| ' + (($Headers | ForEach-Object { '---' }) -join ' | ') + ' |')
    foreach ($row in @($Rows)) {
        [void]$Builder.AppendLine('| ' + (($row | ForEach-Object { Escape-MarkdownCell ([string]$_) }) -join ' | ') + ' |')
    }
    [void]$Builder.AppendLine('')
}

function Add-HtmlTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string[]]$Headers,
        [object[]]$Rows
    )

    [void]$Builder.AppendLine('<table>')
    [void]$Builder.AppendLine('<thead><tr>' + (($Headers | ForEach-Object { '<th>' + (Escape-HtmlText $_) + '</th>' }) -join '') + '</tr></thead>')
    [void]$Builder.AppendLine('<tbody>')
    foreach ($row in @($Rows)) {
        [void]$Builder.AppendLine('<tr>' + (($row | ForEach-Object { '<td>' + (Escape-HtmlText ([string]$_)) + '</td>' }) -join '') + '</tr>')
    }
    [void]$Builder.AppendLine('</tbody>')
    [void]$Builder.AppendLine('</table>')
}

function Get-CaseSummaryRow {
    param(
        $CaseDiff
    )

    $leftNodes = Get-CaseNodeCount -CaseEntry $CaseDiff.left_case
    $rightNodes = Get-CaseNodeCount -CaseEntry $CaseDiff.right_case
    $leftEdges = Get-CaseEdgeCount -CaseEntry $CaseDiff.left_case
    $rightEdges = Get-CaseEdgeCount -CaseEntry $CaseDiff.right_case

    return @(
        [string]$CaseDiff.name,
        [string]$CaseDiff.status,
        "$(Get-CaseKind -CaseEntry $CaseDiff.left_case)->$(Get-CaseKind -CaseEntry $CaseDiff.right_case)",
        "$leftNodes->$rightNodes",
        "$leftEdges->$rightEdges",
        "+$(@($CaseDiff.node_changes.added).Count) -$(@($CaseDiff.node_changes.removed).Count) ~$(@($CaseDiff.node_changes.changed).Count)",
        "+$(@($CaseDiff.edge_changes.added).Count) -$(@($CaseDiff.edge_changes.removed).Count)",
        (Join-Strings -Items @($CaseDiff.summary_changes) -Separator '; '),
        (Join-Strings -Items @($CaseDiff.metadata_changes) -Separator '; ')
    )
}

function Add-MarkdownNodeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string]$TitleText,
        $Nodes
    )

    if (@($Nodes).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine("#### $TitleText")
    [void]$Builder.AppendLine('')
    $rows = @()
    foreach ($node in @($Nodes)) {
        $rows += ,@(
            [string]$node.name,
            [string]$node.kind,
            [string]$node.phase,
            [string]$node.runlevel,
            (Join-Strings -Items @($node.provides)),
            (Join-Strings -Items @($node.requires))
        )
    }
    Add-MarkdownTable -Builder $Builder -Headers @('Name', 'Kind', 'Phase', 'Runlevel', 'Provides', 'Requires') -Rows $rows
}

function Add-HtmlNodeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string]$TitleText,
        $Nodes
    )

    if (@($Nodes).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine("<h4>$(Escape-HtmlText $TitleText)</h4>")
    $rows = @()
    foreach ($node in @($Nodes)) {
        $rows += ,@(
            [string]$node.name,
            [string]$node.kind,
            [string]$node.phase,
            [string]$node.runlevel,
            (Join-Strings -Items @($node.provides)),
            (Join-Strings -Items @($node.requires))
        )
    }
    Add-HtmlTable -Builder $Builder -Headers @('Name', 'Kind', 'Phase', 'Runlevel', 'Provides', 'Requires') -Rows $rows
}

function Add-MarkdownChangedNodeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        $ChangedNodes
    )

    if (@($ChangedNodes).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine('#### Nodes Changed')
    [void]$Builder.AppendLine('')
    $rows = @()
    foreach ($entry in @($ChangedNodes)) {
        $rows += ,@(
            [string]$entry.name,
            (Join-Strings -Items @($entry.fields) -Separator '; '),
            [string]$entry.left.phase,
            [string]$entry.right.phase,
            [string]$entry.left.kind,
            [string]$entry.right.kind
        )
    }
    Add-MarkdownTable -Builder $Builder -Headers @('Name', 'Fields', 'Left Phase', 'Right Phase', 'Left Kind', 'Right Kind') -Rows $rows
}

function Add-HtmlChangedNodeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        $ChangedNodes
    )

    if (@($ChangedNodes).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine('<h4>Nodes Changed</h4>')
    $rows = @()
    foreach ($entry in @($ChangedNodes)) {
        $rows += ,@(
            [string]$entry.name,
            (Join-Strings -Items @($entry.fields) -Separator '; '),
            [string]$entry.left.phase,
            [string]$entry.right.phase,
            [string]$entry.left.kind,
            [string]$entry.right.kind
        )
    }
    Add-HtmlTable -Builder $Builder -Headers @('Name', 'Fields', 'Left Phase', 'Right Phase', 'Left Kind', 'Right Kind') -Rows $rows
}

function Add-MarkdownEdgeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string]$TitleText,
        $Edges
    )

    if (@($Edges).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine("#### $TitleText")
    [void]$Builder.AppendLine('')
    $rows = @()
    foreach ($edge in @($Edges)) {
        $rows += ,@(
            [string]$edge.provider,
            [string]$edge.consumer,
            [string]$edge.capability
        )
    }
    Add-MarkdownTable -Builder $Builder -Headers @('Provider', 'Consumer', 'Capability') -Rows $rows
}

function Add-HtmlEdgeTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [string]$TitleText,
        $Edges
    )

    if (@($Edges).Count -eq 0) {
        return
    }

    [void]$Builder.AppendLine("<h4>$(Escape-HtmlText $TitleText)</h4>")
    $rows = @()
    foreach ($edge in @($Edges)) {
        $rows += ,@(
            [string]$edge.provider,
            [string]$edge.consumer,
            [string]$edge.capability
        )
    }
    Add-HtmlTable -Builder $Builder -Headers @('Provider', 'Consumer', 'Capability') -Rows $rows
}

function Build-MarkdownReport {
    param(
        $DiffData,
        [string]$ReportPath
    )

    $builder = [System.Text.StringBuilder]::new()
    $cases = @($DiffData.cases)

    [void]$builder.AppendLine('# ' + $Title)
    [void]$builder.AppendLine('')
    [void]$builder.AppendLine('- Generated: `' + (Get-Date -Format s) + '`')
    [void]$builder.AppendLine('- Left bundle: `' + (Escape-MarkdownCell ([string]$DiffData.left.index)) + '`')
    if ($null -ne $DiffData.left.PSObject.Properties['input_manifest'] -and $null -ne $DiffData.left.input_manifest -and -not [string]::IsNullOrWhiteSpace([string]$DiffData.left.input_manifest.path)) {
        [void]$builder.AppendLine('- Left input manifest: `' + (Escape-MarkdownCell ([string]$DiffData.left.input_manifest.path)) + '`')
    }
    if ($null -ne $DiffData.left.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.left.declared_facts_defaults -and @($DiffData.left.declared_facts_defaults).Count -gt 0) {
        [void]$builder.AppendLine('- Left declared facts defaults: `' + (Escape-MarkdownCell ((@($DiffData.left.declared_facts_defaults) -join ', '))) + '`')
    }
    if ($null -ne $DiffData.left.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.left.declared_contracts_defaults -and @($DiffData.left.declared_contracts_defaults).Count -gt 0) {
        [void]$builder.AppendLine('- Left declared contracts defaults: `' + (Escape-MarkdownCell ((@(Format-DeclaredContracts -Contracts $DiffData.left.declared_contracts_defaults) -join '; '))) + '`')
    }
    [void]$builder.AppendLine('- Right bundle: `' + (Escape-MarkdownCell ([string]$DiffData.right.index)) + '`')
    if ($null -ne $DiffData.right.PSObject.Properties['input_manifest'] -and $null -ne $DiffData.right.input_manifest -and -not [string]::IsNullOrWhiteSpace([string]$DiffData.right.input_manifest.path)) {
        [void]$builder.AppendLine('- Right input manifest: `' + (Escape-MarkdownCell ([string]$DiffData.right.input_manifest.path)) + '`')
    }
    if ($null -ne $DiffData.right.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.right.declared_facts_defaults -and @($DiffData.right.declared_facts_defaults).Count -gt 0) {
        [void]$builder.AppendLine('- Right declared facts defaults: `' + (Escape-MarkdownCell ((@($DiffData.right.declared_facts_defaults) -join ', '))) + '`')
    }
    if ($null -ne $DiffData.right.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.right.declared_contracts_defaults -and @($DiffData.right.declared_contracts_defaults).Count -gt 0) {
        [void]$builder.AppendLine('- Right declared contracts defaults: `' + (Escape-MarkdownCell ((@(Format-DeclaredContracts -Contracts $DiffData.right.declared_contracts_defaults) -join '; '))) + '`')
    }
    [void]$builder.AppendLine('- Cases in report: `' + [string]$DiffData.case_count + '`')
    [void]$builder.AppendLine('- Changed: `' + (Get-CaseStatusCount -Cases $cases -Status 'changed') + '`, Added: `' + (Get-CaseStatusCount -Cases $cases -Status 'added') + '`, Removed: `' + (Get-CaseStatusCount -Cases $cases -Status 'removed') + '`, Unchanged: `' + (Get-CaseStatusCount -Cases $cases -Status 'unchanged') + '`')
    [void]$builder.AppendLine('')

    [void]$builder.AppendLine('## Summary')
    [void]$builder.AppendLine('')
    $summaryRows = @()
    foreach ($caseDiff in $cases) {
        $summaryRows += ,(Get-CaseSummaryRow -CaseDiff $caseDiff)
    }
    if ($summaryRows.Count -gt 0) {
        Add-MarkdownTable -Builder $builder -Headers @('Case', 'Status', 'Kind', 'Nodes', 'Edges', 'Node Delta', 'Edge Delta', 'Summary', 'Metadata') -Rows $summaryRows
    } else {
        [void]$builder.AppendLine('No visible differences.')
        [void]$builder.AppendLine('')
    }

    foreach ($caseDiff in $cases) {
        $leftKind = Get-CaseKind -CaseEntry $caseDiff.left_case
        $rightKind = Get-CaseKind -CaseEntry $caseDiff.right_case
        [void]$builder.AppendLine('## Case: ' + [string]$caseDiff.name)
        [void]$builder.AppendLine('')
        [void]$builder.AppendLine('- Status: `' + [string]$caseDiff.status + '`')
        [void]$builder.AppendLine('- Kind: `' + (Escape-MarkdownCell ($leftKind + ' -> ' + $rightKind)) + '`')
        if (@($caseDiff.summary_changes).Count -gt 0) {
            [void]$builder.AppendLine('- Summary changes: `' + (Escape-MarkdownCell (Join-Strings -Items @($caseDiff.summary_changes) -Separator '; ')) + '`')
        }
        if (@($caseDiff.metadata_changes).Count -gt 0) {
            [void]$builder.AppendLine('- Metadata changes: `' + (Escape-MarkdownCell (Join-Strings -Items @($caseDiff.metadata_changes) -Separator '; ')) + '`')
        }
        if ($null -ne $caseDiff.left_case -and @($caseDiff.left_case.declared_facts).Count -gt 0) {
            [void]$builder.AppendLine('- Left declared facts: `' + (Escape-MarkdownCell ((@($caseDiff.left_case.declared_facts) -join ', '))) + '`')
        }
        if ($null -ne $caseDiff.right_case -and @($caseDiff.right_case.declared_facts).Count -gt 0) {
            [void]$builder.AppendLine('- Right declared facts: `' + (Escape-MarkdownCell ((@($caseDiff.right_case.declared_facts) -join ', '))) + '`')
        }
        if ($null -ne $caseDiff.left_case -and @($caseDiff.left_case.declared_contracts).Count -gt 0) {
            [void]$builder.AppendLine('- Left declared contracts: `' + (Escape-MarkdownCell ((@(Format-DeclaredContracts -Contracts $caseDiff.left_case.declared_contracts) -join '; '))) + '`')
        }
        if ($null -ne $caseDiff.right_case -and @($caseDiff.right_case.declared_contracts).Count -gt 0) {
            [void]$builder.AppendLine('- Right declared contracts: `' + (Escape-MarkdownCell ((@(Format-DeclaredContracts -Contracts $caseDiff.right_case.declared_contracts) -join '; '))) + '`')
        }

        $leftLinks = @()
        $rightLinks = @()
        $leftDotLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'dot' -Label 'dot'
        $leftJsonLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'json' -Label 'json'
        $leftRuntimeObserveLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'runtime_observe' -Label 'runtime_observe'
        $rightDotLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'dot' -Label 'dot'
        $rightJsonLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'json' -Label 'json'
        $rightRuntimeObserveLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'runtime_observe' -Label 'runtime_observe'
        if (-not [string]::IsNullOrWhiteSpace($leftDotLink)) { $leftLinks += $leftDotLink }
        if (-not [string]::IsNullOrWhiteSpace($leftJsonLink)) { $leftLinks += $leftJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($leftRuntimeObserveLink)) { $leftLinks += $leftRuntimeObserveLink }
        if (-not [string]::IsNullOrWhiteSpace($rightDotLink)) { $rightLinks += $rightDotLink }
        if (-not [string]::IsNullOrWhiteSpace($rightJsonLink)) { $rightLinks += $rightJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($rightRuntimeObserveLink)) { $rightLinks += $rightRuntimeObserveLink }
        if ($leftLinks.Count -gt 0) {
            [void]$builder.AppendLine("- Left artifacts: $(Join-Strings -Items $leftLinks)")
        } elseif ($leftKind -ne 'absent' -and $null -eq (Get-CaseGraphSummary -CaseEntry $caseDiff.left_case)) {
            [void]$builder.AppendLine('- Left artifacts: no static graph')
        }
        if ($rightLinks.Count -gt 0) {
            [void]$builder.AppendLine("- Right artifacts: $(Join-Strings -Items $rightLinks)")
        } elseif ($rightKind -ne 'absent' -and $null -eq (Get-CaseGraphSummary -CaseEntry $caseDiff.right_case)) {
            [void]$builder.AppendLine('- Right artifacts: no static graph')
        }
        [void]$builder.AppendLine('')

        Add-MarkdownNodeTable -Builder $builder -TitleText 'Nodes Added' -Nodes $caseDiff.node_changes.added
        Add-MarkdownNodeTable -Builder $builder -TitleText 'Nodes Removed' -Nodes $caseDiff.node_changes.removed
        Add-MarkdownChangedNodeTable -Builder $builder -ChangedNodes $caseDiff.node_changes.changed
        Add-MarkdownEdgeTable -Builder $builder -TitleText 'Edges Added' -Edges $caseDiff.edge_changes.added
        Add-MarkdownEdgeTable -Builder $builder -TitleText 'Edges Removed' -Edges $caseDiff.edge_changes.removed

        if (@($caseDiff.node_changes.added).Count -eq 0 -and @($caseDiff.node_changes.removed).Count -eq 0 -and @($caseDiff.node_changes.changed).Count -eq 0 -and @($caseDiff.edge_changes.added).Count -eq 0 -and @($caseDiff.edge_changes.removed).Count -eq 0 -and @($caseDiff.metadata_changes).Count -eq 0) {
            [void]$builder.AppendLine('No detailed differences.')
            [void]$builder.AppendLine('')
        }
    }

    return $builder.ToString()
}

function Build-HtmlReport {
    param(
        $DiffData,
        [string]$ReportPath
    )

    $builder = [System.Text.StringBuilder]::new()
    $cases = @($DiffData.cases)

    [void]$builder.AppendLine('<!DOCTYPE html>')
    [void]$builder.AppendLine('<html lang=en>')
    [void]$builder.AppendLine('<head>')
    [void]$builder.AppendLine('<meta charset=utf-8>')
    [void]$builder.AppendLine("<title>$(Escape-HtmlText $Title)</title>")
    [void]$builder.AppendLine('<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.5;color:#1f2328}h1,h2,h3,h4{margin-top:1.2em}table{border-collapse:collapse;width:100%;margin:12px 0 20px 0}th,td{border:1px solid #d0d7de;padding:6px 8px;vertical-align:top;text-align:left}th{background:#f6f8fa}code{font-family:Consolas,monospace;background:#f6f8fa;padding:1px 4px;border-radius:4px}details{border:1px solid #d0d7de;border-radius:6px;padding:10px 12px;margin:16px 0}summary{cursor:pointer;font-weight:600}.meta{color:#57606a}.status-added{color:#1a7f37}.status-removed{color:#cf222e}.status-changed{color:#9a6700}.status-unchanged{color:#57606a}</style>')
    [void]$builder.AppendLine('</head>')
    [void]$builder.AppendLine('<body>')
    [void]$builder.AppendLine("<h1>$(Escape-HtmlText $Title)</h1>")
    [void]$builder.AppendLine('<ul class=meta>')
    [void]$builder.AppendLine("<li>Generated: <code>$(Escape-HtmlText ((Get-Date -Format s)))</code></li>")
    [void]$builder.AppendLine("<li>Left bundle: <code>$(Escape-HtmlText ([string]$DiffData.left.index))</code></li>")
    if ($null -ne $DiffData.left.PSObject.Properties['input_manifest'] -and $null -ne $DiffData.left.input_manifest -and -not [string]::IsNullOrWhiteSpace([string]$DiffData.left.input_manifest.path)) {
        [void]$builder.AppendLine("<li>Left input manifest: <code>$(Escape-HtmlText ([string]$DiffData.left.input_manifest.path))</code></li>")
    }
    if ($null -ne $DiffData.left.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.left.declared_facts_defaults -and @($DiffData.left.declared_facts_defaults).Count -gt 0) {
        [void]$builder.AppendLine("<li>Left declared facts defaults: <code>$(Escape-HtmlText ((@($DiffData.left.declared_facts_defaults) -join ', ')))</code></li>")
    }
    if ($null -ne $DiffData.left.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.left.declared_contracts_defaults -and @($DiffData.left.declared_contracts_defaults).Count -gt 0) {
        [void]$builder.AppendLine("<li>Left declared contracts defaults: <code>$(Escape-HtmlText ((@(Format-DeclaredContracts -Contracts $DiffData.left.declared_contracts_defaults) -join '; ')))</code></li>")
    }
    [void]$builder.AppendLine("<li>Right bundle: <code>$(Escape-HtmlText ([string]$DiffData.right.index))</code></li>")
    if ($null -ne $DiffData.right.PSObject.Properties['input_manifest'] -and $null -ne $DiffData.right.input_manifest -and -not [string]::IsNullOrWhiteSpace([string]$DiffData.right.input_manifest.path)) {
        [void]$builder.AppendLine("<li>Right input manifest: <code>$(Escape-HtmlText ([string]$DiffData.right.input_manifest.path))</code></li>")
    }
    if ($null -ne $DiffData.right.PSObject.Properties['declared_facts_defaults'] -and $null -ne $DiffData.right.declared_facts_defaults -and @($DiffData.right.declared_facts_defaults).Count -gt 0) {
        [void]$builder.AppendLine("<li>Right declared facts defaults: <code>$(Escape-HtmlText ((@($DiffData.right.declared_facts_defaults) -join ', ')))</code></li>")
    }
    if ($null -ne $DiffData.right.PSObject.Properties['declared_contracts_defaults'] -and $null -ne $DiffData.right.declared_contracts_defaults -and @($DiffData.right.declared_contracts_defaults).Count -gt 0) {
        [void]$builder.AppendLine("<li>Right declared contracts defaults: <code>$(Escape-HtmlText ((@(Format-DeclaredContracts -Contracts $DiffData.right.declared_contracts_defaults) -join '; ')))</code></li>")
    }
    [void]$builder.AppendLine("<li>Cases in report: <code>$($DiffData.case_count)</code></li>")
    [void]$builder.AppendLine("<li>Changed: <code>$(Get-CaseStatusCount -Cases $cases -Status 'changed')</code>, Added: <code>$(Get-CaseStatusCount -Cases $cases -Status 'added')</code>, Removed: <code>$(Get-CaseStatusCount -Cases $cases -Status 'removed')</code>, Unchanged: <code>$(Get-CaseStatusCount -Cases $cases -Status 'unchanged')</code></li>")
    [void]$builder.AppendLine('</ul>')

    [void]$builder.AppendLine('<h2>Summary</h2>')
    $summaryRows = @()
    foreach ($caseDiff in $cases) {
        $summaryRows += ,(Get-CaseSummaryRow -CaseDiff $caseDiff)
    }
    if ($summaryRows.Count -gt 0) {
        Add-HtmlTable -Builder $builder -Headers @('Case', 'Status', 'Kind', 'Nodes', 'Edges', 'Node Delta', 'Edge Delta', 'Summary', 'Metadata') -Rows $summaryRows
    } else {
        [void]$builder.AppendLine('<p>No visible differences.</p>')
    }

    foreach ($caseDiff in $cases) {
        $leftKind = Get-CaseKind -CaseEntry $caseDiff.left_case
        $rightKind = Get-CaseKind -CaseEntry $caseDiff.right_case
        $statusClass = 'status-' + [string]$caseDiff.status
        [void]$builder.AppendLine('<details open>')
        [void]$builder.AppendLine('<summary><span class=' + (Escape-HtmlText $statusClass) + '>' + (Escape-HtmlText ([string]$caseDiff.name)) + '</span> &mdash; ' + (Escape-HtmlText ([string]$caseDiff.status)) + '</summary>')
        [void]$builder.AppendLine('<div>')
        [void]$builder.AppendLine("<p><strong>Kind:</strong> <code>$(Escape-HtmlText ($leftKind + ' -> ' + $rightKind))</code></p>")
        if (@($caseDiff.summary_changes).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Summary changes:</strong> $(Escape-HtmlText (Join-Strings -Items @($caseDiff.summary_changes) -Separator '; '))</p>")
        }
        if (@($caseDiff.metadata_changes).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Metadata changes:</strong> $(Escape-HtmlText (Join-Strings -Items @($caseDiff.metadata_changes) -Separator '; '))</p>")
        }
        if ($null -ne $caseDiff.left_case -and @($caseDiff.left_case.declared_facts).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Left declared facts:</strong> <code>$(Escape-HtmlText ((@($caseDiff.left_case.declared_facts) -join ', ')))</code></p>")
        }
        if ($null -ne $caseDiff.right_case -and @($caseDiff.right_case.declared_facts).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Right declared facts:</strong> <code>$(Escape-HtmlText ((@($caseDiff.right_case.declared_facts) -join ', ')))</code></p>")
        }
        if ($null -ne $caseDiff.left_case -and @($caseDiff.left_case.declared_contracts).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Left declared contracts:</strong> <code>$(Escape-HtmlText ((@(Format-DeclaredContracts -Contracts $caseDiff.left_case.declared_contracts) -join '; ')))</code></p>")
        }
        if ($null -ne $caseDiff.right_case -and @($caseDiff.right_case.declared_contracts).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Right declared contracts:</strong> <code>$(Escape-HtmlText ((@(Format-DeclaredContracts -Contracts $caseDiff.right_case.declared_contracts) -join '; ')))</code></p>")
        }

        $leftLinks = @()
        $rightLinks = @()
        $leftDotLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'dot' -Label 'dot'
        $leftJsonLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'json' -Label 'json'
        $leftRuntimeObserveLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'runtime_observe' -Label 'runtime_observe'
        $rightDotLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'dot' -Label 'dot'
        $rightJsonLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'json' -Label 'json'
        $rightRuntimeObserveLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'runtime_observe' -Label 'runtime_observe'
        if (-not [string]::IsNullOrWhiteSpace($leftDotLink)) { $leftLinks += $leftDotLink }
        if (-not [string]::IsNullOrWhiteSpace($leftJsonLink)) { $leftLinks += $leftJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($leftRuntimeObserveLink)) { $leftLinks += $leftRuntimeObserveLink }
        if (-not [string]::IsNullOrWhiteSpace($rightDotLink)) { $rightLinks += $rightDotLink }
        if (-not [string]::IsNullOrWhiteSpace($rightJsonLink)) { $rightLinks += $rightJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($rightRuntimeObserveLink)) { $rightLinks += $rightRuntimeObserveLink }
        if ($leftLinks.Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Left artifacts:</strong> $(Join-Strings -Items $leftLinks)</p>")
        } elseif ($leftKind -ne 'absent' -and $null -eq (Get-CaseGraphSummary -CaseEntry $caseDiff.left_case)) {
            [void]$builder.AppendLine('<p><strong>Left artifacts:</strong> no static graph</p>')
        }
        if ($rightLinks.Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Right artifacts:</strong> $(Join-Strings -Items $rightLinks)</p>")
        } elseif ($rightKind -ne 'absent' -and $null -eq (Get-CaseGraphSummary -CaseEntry $caseDiff.right_case)) {
            [void]$builder.AppendLine('<p><strong>Right artifacts:</strong> no static graph</p>')
        }

        Add-HtmlNodeTable -Builder $builder -TitleText 'Nodes Added' -Nodes $caseDiff.node_changes.added
        Add-HtmlNodeTable -Builder $builder -TitleText 'Nodes Removed' -Nodes $caseDiff.node_changes.removed
        Add-HtmlChangedNodeTable -Builder $builder -ChangedNodes $caseDiff.node_changes.changed
        Add-HtmlEdgeTable -Builder $builder -TitleText 'Edges Added' -Edges $caseDiff.edge_changes.added
        Add-HtmlEdgeTable -Builder $builder -TitleText 'Edges Removed' -Edges $caseDiff.edge_changes.removed

        if (@($caseDiff.node_changes.added).Count -eq 0 -and @($caseDiff.node_changes.removed).Count -eq 0 -and @($caseDiff.node_changes.changed).Count -eq 0 -and @($caseDiff.edge_changes.added).Count -eq 0 -and @($caseDiff.edge_changes.removed).Count -eq 0 -and @($caseDiff.metadata_changes).Count -eq 0) {
            [void]$builder.AppendLine('<p>No detailed differences.</p>')
        }

        [void]$builder.AppendLine('</div>')
        [void]$builder.AppendLine('</details>')
    }

    [void]$builder.AppendLine('</body>')
    [void]$builder.AppendLine('</html>')
    return $builder.ToString()
}

$diffData = Get-DiffData
$writeMarkdown = $Format -eq 'markdown' -or $Format -eq 'both'
$writeHtml = $Format -eq 'html' -or $Format -eq 'both'

$resolvedMarkdownPath = ''
$resolvedHtmlPath = ''
$writtenPaths = @()
if ($writeMarkdown) {
    $resolvedMarkdownPath = Get-OutputPath -ExplicitPath $MarkdownPath -DefaultFileName 'materialized_graph_bundle_diff_report.md'
    Ensure-ParentDirectory -Path $resolvedMarkdownPath
    $markdown = Build-MarkdownReport -DiffData $diffData -ReportPath $resolvedMarkdownPath
    Set-Content -LiteralPath $resolvedMarkdownPath -Encoding utf8 $markdown
    $writtenPaths += "[MD] $resolvedMarkdownPath"
}

if ($writeHtml) {
    $resolvedHtmlPath = Get-OutputPath -ExplicitPath $HtmlPath -DefaultFileName 'materialized_graph_bundle_diff_report.html'
    Ensure-ParentDirectory -Path $resolvedHtmlPath
    $html = Build-HtmlReport -DiffData $diffData -ReportPath $resolvedHtmlPath
    Set-Content -LiteralPath $resolvedHtmlPath -Encoding utf8 $html
    $writtenPaths += "[HTML] $resolvedHtmlPath"
}

$resolvedManifestPath = Get-ManifestOutputPath
Ensure-ParentDirectory -Path $resolvedManifestPath
$manifest = New-ReportManifest -DiffData $diffData -ResolvedMarkdownPath $resolvedMarkdownPath -ResolvedHtmlPath $resolvedHtmlPath -ResolvedManifestPath $resolvedManifestPath
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedManifestPath -Encoding utf8
$writtenPaths += "[MANIFEST] $resolvedManifestPath"

foreach ($line in $writtenPaths) {
    Write-Host $line
}
