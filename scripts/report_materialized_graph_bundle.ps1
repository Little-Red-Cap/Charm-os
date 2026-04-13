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
        }
        right = [ordered]@{
            index = [string]$DiffData.right.index
            bundle_root = [string]$DiffData.right.bundle_root
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

    $leftNodes = if ($null -ne $CaseDiff.left_case) { [int]$CaseDiff.left_case.graph.node_count } else { 0 }
    $rightNodes = if ($null -ne $CaseDiff.right_case) { [int]$CaseDiff.right_case.graph.node_count } else { 0 }
    $leftEdges = if ($null -ne $CaseDiff.left_case) { [int]$CaseDiff.left_case.graph.edge_count } else { 0 }
    $rightEdges = if ($null -ne $CaseDiff.right_case) { [int]$CaseDiff.right_case.graph.edge_count } else { 0 }

    return @(
        [string]$CaseDiff.name,
        [string]$CaseDiff.status,
        "$leftNodes->$rightNodes",
        "$leftEdges->$rightEdges",
        "+$(@($CaseDiff.node_changes.added).Count) -$(@($CaseDiff.node_changes.removed).Count) ~$(@($CaseDiff.node_changes.changed).Count)",
        "+$(@($CaseDiff.edge_changes.added).Count) -$(@($CaseDiff.edge_changes.removed).Count)",
        (Join-Strings -Items @($CaseDiff.summary_changes) -Separator '; ')
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
    [void]$builder.AppendLine('- Right bundle: `' + (Escape-MarkdownCell ([string]$DiffData.right.index)) + '`')
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
        Add-MarkdownTable -Builder $builder -Headers @('Case', 'Status', 'Nodes', 'Edges', 'Node Delta', 'Edge Delta', 'Summary') -Rows $summaryRows
    } else {
        [void]$builder.AppendLine('No visible differences.')
        [void]$builder.AppendLine('')
    }

    foreach ($caseDiff in $cases) {
        [void]$builder.AppendLine('## Case: ' + [string]$caseDiff.name)
        [void]$builder.AppendLine('')
        [void]$builder.AppendLine('- Status: `' + [string]$caseDiff.status + '`')
        if (@($caseDiff.summary_changes).Count -gt 0) {
            [void]$builder.AppendLine('- Summary changes: `' + (Escape-MarkdownCell (Join-Strings -Items @($caseDiff.summary_changes) -Separator '; ')) + '`')
        }

        $leftLinks = @()
        $rightLinks = @()
        $leftDotLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'dot' -Label 'dot'
        $leftJsonLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'json' -Label 'json'
        $rightDotLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'dot' -Label 'dot'
        $rightJsonLink = Get-ArtifactLinkMarkdown -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'json' -Label 'json'
        if (-not [string]::IsNullOrWhiteSpace($leftDotLink)) { $leftLinks += $leftDotLink }
        if (-not [string]::IsNullOrWhiteSpace($leftJsonLink)) { $leftLinks += $leftJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($rightDotLink)) { $rightLinks += $rightDotLink }
        if (-not [string]::IsNullOrWhiteSpace($rightJsonLink)) { $rightLinks += $rightJsonLink }
        if ($leftLinks.Count -gt 0) {
            [void]$builder.AppendLine("- Left artifacts: $(Join-Strings -Items $leftLinks)")
        }
        if ($rightLinks.Count -gt 0) {
            [void]$builder.AppendLine("- Right artifacts: $(Join-Strings -Items $rightLinks)")
        }
        [void]$builder.AppendLine('')

        Add-MarkdownNodeTable -Builder $builder -TitleText 'Nodes Added' -Nodes $caseDiff.node_changes.added
        Add-MarkdownNodeTable -Builder $builder -TitleText 'Nodes Removed' -Nodes $caseDiff.node_changes.removed
        Add-MarkdownChangedNodeTable -Builder $builder -ChangedNodes $caseDiff.node_changes.changed
        Add-MarkdownEdgeTable -Builder $builder -TitleText 'Edges Added' -Edges $caseDiff.edge_changes.added
        Add-MarkdownEdgeTable -Builder $builder -TitleText 'Edges Removed' -Edges $caseDiff.edge_changes.removed

        if (@($caseDiff.node_changes.added).Count -eq 0 -and @($caseDiff.node_changes.removed).Count -eq 0 -and @($caseDiff.node_changes.changed).Count -eq 0 -and @($caseDiff.edge_changes.added).Count -eq 0 -and @($caseDiff.edge_changes.removed).Count -eq 0) {
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
    [void]$builder.AppendLine("<li>Right bundle: <code>$(Escape-HtmlText ([string]$DiffData.right.index))</code></li>")
    [void]$builder.AppendLine("<li>Cases in report: <code>$($DiffData.case_count)</code></li>")
    [void]$builder.AppendLine("<li>Changed: <code>$(Get-CaseStatusCount -Cases $cases -Status 'changed')</code>, Added: <code>$(Get-CaseStatusCount -Cases $cases -Status 'added')</code>, Removed: <code>$(Get-CaseStatusCount -Cases $cases -Status 'removed')</code>, Unchanged: <code>$(Get-CaseStatusCount -Cases $cases -Status 'unchanged')</code></li>")
    [void]$builder.AppendLine('</ul>')

    [void]$builder.AppendLine('<h2>Summary</h2>')
    $summaryRows = @()
    foreach ($caseDiff in $cases) {
        $summaryRows += ,(Get-CaseSummaryRow -CaseDiff $caseDiff)
    }
    if ($summaryRows.Count -gt 0) {
        Add-HtmlTable -Builder $builder -Headers @('Case', 'Status', 'Nodes', 'Edges', 'Node Delta', 'Edge Delta', 'Summary') -Rows $summaryRows
    } else {
        [void]$builder.AppendLine('<p>No visible differences.</p>')
    }

    foreach ($caseDiff in $cases) {
        $statusClass = 'status-' + [string]$caseDiff.status
        [void]$builder.AppendLine('<details open>')
        [void]$builder.AppendLine('<summary><span class=' + (Escape-HtmlText $statusClass) + '>' + (Escape-HtmlText ([string]$caseDiff.name)) + '</span> &mdash; ' + (Escape-HtmlText ([string]$caseDiff.status)) + '</summary>')
        [void]$builder.AppendLine('<div>')
        if (@($caseDiff.summary_changes).Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Summary changes:</strong> $(Escape-HtmlText (Join-Strings -Items @($caseDiff.summary_changes) -Separator '; '))</p>")
        }

        $leftLinks = @()
        $rightLinks = @()
        $leftDotLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'dot' -Label 'dot'
        $leftJsonLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.left_case -FieldName 'json' -Label 'json'
        $rightDotLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'dot' -Label 'dot'
        $rightJsonLink = Get-ArtifactLinkHtml -ReportPath $ReportPath -CaseEntry $caseDiff.right_case -FieldName 'json' -Label 'json'
        if (-not [string]::IsNullOrWhiteSpace($leftDotLink)) { $leftLinks += $leftDotLink }
        if (-not [string]::IsNullOrWhiteSpace($leftJsonLink)) { $leftLinks += $leftJsonLink }
        if (-not [string]::IsNullOrWhiteSpace($rightDotLink)) { $rightLinks += $rightDotLink }
        if (-not [string]::IsNullOrWhiteSpace($rightJsonLink)) { $rightLinks += $rightJsonLink }
        if ($leftLinks.Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Left artifacts:</strong> $(Join-Strings -Items $leftLinks)</p>")
        }
        if ($rightLinks.Count -gt 0) {
            [void]$builder.AppendLine("<p><strong>Right artifacts:</strong> $(Join-Strings -Items $rightLinks)</p>")
        }

        Add-HtmlNodeTable -Builder $builder -TitleText 'Nodes Added' -Nodes $caseDiff.node_changes.added
        Add-HtmlNodeTable -Builder $builder -TitleText 'Nodes Removed' -Nodes $caseDiff.node_changes.removed
        Add-HtmlChangedNodeTable -Builder $builder -ChangedNodes $caseDiff.node_changes.changed
        Add-HtmlEdgeTable -Builder $builder -TitleText 'Edges Added' -Edges $caseDiff.edge_changes.added
        Add-HtmlEdgeTable -Builder $builder -TitleText 'Edges Removed' -Edges $caseDiff.edge_changes.removed

        if (@($caseDiff.node_changes.added).Count -eq 0 -and @($caseDiff.node_changes.removed).Count -eq 0 -and @($caseDiff.node_changes.changed).Count -eq 0 -and @($caseDiff.edge_changes.added).Count -eq 0 -and @($caseDiff.edge_changes.removed).Count -eq 0) {
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
