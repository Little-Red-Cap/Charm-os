param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$Title = "Compiler Lifecycle Summary Report"
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

function Test-ObjectProperty {
    param(
        $Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $false
    }

    return $null -ne $Object.PSObject.Properties[$Name]
}

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name,
        $Default = $null
    )

    if (-not (Test-ObjectProperty -Object $Object -Name $Name)) {
        return $Default
    }

    $value = $Object.PSObject.Properties[$Name].Value
    if ($null -eq $value) {
        return $Default
    }

    return $value
}

function Get-ObjectStringValue {
    param(
        $Object,
        [string]$Name,
        [string]$Default = ""
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [string]$value
}

function Escape-MarkdownCell {
    param(
        [string]$Text
    )

    if ($null -eq $Text) {
        return ""
    }

    $escaped = $Text.Replace("|", "\|").Replace("`r", "")
    return $escaped.Replace("`n", "<br/>")
}

function Join-StringValues {
    param(
        [object[]]$Values,
        [string]$Separator = ", "
    )

    $items = @(
        @($Values) |
            ForEach-Object { [string]$_ } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    return ($items -join $Separator)
}

function Format-SourceSurfaces {
    param(
        $State
    )

    $surfaces = @(Get-ObjectPropertyValue -Object $State -Name "source_surfaces" -Default @())
    if ($surfaces.Count -eq 0) {
        return "none"
    }

    $labels = @(
        $surfaces |
            ForEach-Object {
                $kind = Get-ObjectStringValue -Object $_ -Name "kind" -Default "unknown"
                $status = Get-ObjectStringValue -Object $_ -Name "status" -Default "unknown"
                "$($kind):$($status)"
            }
    )

    return Join-StringValues -Values $labels -Separator "; "
}

function Format-StateNotes {
    param(
        $State
    )

    $notes = @(Get-ObjectPropertyValue -Object $State -Name "notes" -Default @())
    if ($notes.Count -eq 0) {
        return ""
    }

    return Join-StringValues -Values $notes -Separator "; "
}

$summaryPath = if ([string]::IsNullOrWhiteSpace($Summary)) {
    Resolve-FullPath -Path "out/compiler-lifecycle-summary/compiler_lifecycle.summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "compiler lifecycle summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
if ([string]$summaryData.schema -ne "charm.compiler_lifecycle.summary/v0") {
    throw "unsupported compiler lifecycle summary schema: $([string]$summaryData.schema)"
}

$outputPath = if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    Join-Path (Split-Path -Parent $summaryPath) "compiler_lifecycle.summary.report.md"
} else {
    Resolve-FullPath -Path $OutputPath
}

$states = Get-ObjectPropertyValue -Object $summaryData -Name "states" -Default $null
$stateOrder = @(
    "declared",
    "materialized",
    "proven",
    "frozen",
    "lowered",
    "witnessed",
    "observed",
    "archived",
    "compared"
)

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine(("# {0}" -f $Title))
[void]$builder.AppendLine("")
[void]$builder.AppendLine(('- Summary: `{0}`' -f $summaryPath))
[void]$builder.AppendLine(('- Schema: `{0}`' -f [string]$summaryData.schema))
[void]$builder.AppendLine(('- Kind: `{0}`' -f [string]$summaryData.kind))
[void]$builder.AppendLine(('- Result: `{0}`' -f [string]$summaryData.result))
[void]$builder.AppendLine(('- Generated at: `{0}`' -f [string]$summaryData.generated_at_utc))
[void]$builder.AppendLine(('- State count: `{0}`' -f [string]$summaryData.state_count))
[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Honesty Markers")
[void]$builder.AppendLine("")

$frozen = Get-ObjectPropertyValue -Object $states -Name "frozen" -Default $null
$lowered = Get-ObjectPropertyValue -Object $states -Name "lowered" -Default $null
$archived = Get-ObjectPropertyValue -Object $states -Name "archived" -Default $null

[void]$builder.AppendLine(('- Frozen: `{0} / {1} / {2}`' -f (Get-ObjectStringValue -Object $frozen -Name "status"), (Get-ObjectStringValue -Object $frozen -Name "projection_kind"), (Get-ObjectStringValue -Object $frozen -Name "sidecar_gap")))
[void]$builder.AppendLine(('- Lowered projection: `{0}`' -f (Get-ObjectStringValue -Object $lowered -Name "projection_kind")))
[void]$builder.AppendLine(('- Archived coverage: `{0}`' -f (Get-ObjectStringValue -Object $archived -Name "coverage_strength")))
[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Lifecycle States")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("State | Status | Coverage | Projection | Sidecar gap | Sources | Notes")
[void]$builder.AppendLine("--- | --- | --- | --- | --- | --- | ---")

foreach ($stateName in $stateOrder) {
    $state = Get-ObjectPropertyValue -Object $states -Name $stateName -Default $null
    [void]$builder.AppendLine(("{0} | {1} | {2} | {3} | {4} | {5} | {6}" -f
        (Escape-MarkdownCell -Text $stateName),
        (Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $state -Name "status")),
        (Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $state -Name "coverage_strength")),
        (Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $state -Name "projection_kind")),
        (Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $state -Name "sidecar_gap")),
        (Escape-MarkdownCell -Text (Format-SourceSurfaces -State $state)),
        (Escape-MarkdownCell -Text (Format-StateNotes -State $state))
    ))
}

$violations = @(Get-ObjectPropertyValue -Object $summaryData -Name "violations" -Default @())
[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Summary Violations")
[void]$builder.AppendLine("")
if ($violations.Count -eq 0) {
    [void]$builder.AppendLine("- none")
} else {
    foreach ($violation in $violations) {
        [void]$builder.AppendLine(('- `{0}`' -f [string]$violation))
    }
}

[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Consumer Boundary")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("This report is a read-only rendering of `compiler_lifecycle.summary.json`.")
[void]$builder.AppendLine("It does not parse raw logs, rerun the exporter, modify verdicts, or create source facts.")

Ensure-ParentDirectory -Path $outputPath
Set-Content -LiteralPath $outputPath -Encoding utf8 ($builder.ToString())
Write-Host ("[MD] {0}" -f $outputPath)
