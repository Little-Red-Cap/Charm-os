param(
    [string]$Source = "Examples/init/materialize_observe_demo",
    [string]$BuildDir = "cmake-build-init-observe-demo-clang",
    [string]$CCompiler = "clang",
    [string]$CxxCompiler = "clang++",
    [string]$BuildTarget = "init-materialize-observe-demo",
    [string]$ExportTarget = "export_materialized_graph_demo",
    [string]$Dot = "",
    [string]$Json = "",
    [int]$Jobs = 8,
    [switch]$Clean,
    [switch]$ConfigureOnly,
    [string[]]$Case = @(),
    [switch]$AllCases,
    [switch]$ListCases,
    [string]$OutputRoot = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$exportCaseManifestPath = Join-Path $PSScriptRoot 'materialized_graph.export_case_manifest.v1.json'
. (Join-Path $PSScriptRoot 'materialized_graph_schema.ps1')

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$PropertyName
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Get-RequiredStringProperty {
    param(
        $Object,
        [string]$PropertyName,
        [string]$Context
    )

    $value = Get-ObjectPropertyValue -Object $Object -PropertyName $PropertyName
    if ([string]::IsNullOrWhiteSpace([string]$value)) {
        throw "missing required string property '$PropertyName' in $Context"
    }

    return [string]$value
}

function Get-OptionalStringProperty {
    param(
        $Object,
        [string]$PropertyName
    )

    $value = Get-ObjectPropertyValue -Object $Object -PropertyName $PropertyName
    if ([string]::IsNullOrWhiteSpace([string]$value)) {
        return $null
    }

    return [string]$value
}

function Get-OptionalStringArrayProperty {
    param(
        $Object,
        [string]$PropertyName
    )

    $value = Get-ObjectPropertyValue -Object $Object -PropertyName $PropertyName
    if ($null -eq $value) {
        return @()
    }

    return @(
        @($value) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )
}

function Convert-ExportCaseEntry {
    param(
        $CaseEntry,
        [string]$ManifestPath,
        [int]$Index
    )

    $context = "$ManifestPath cases[$Index]"
    $subjectEntry = Get-ObjectPropertyValue -Object $CaseEntry -PropertyName 'subject'

    return [pscustomobject]@{
        Name = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'name' -Context $context
        Source = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'source' -Context $context
        BuildDir = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'build_dir' -Context $context
        BuildTarget = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'build_target' -Context $context
        ExportTarget = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'export_target'
        DotCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'dot_cache'
        JsonCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'json_cache'
        DefaultDot = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'default_dot' -Context $context
        DefaultJson = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'default_json' -Context $context
        ExtraCache = Get-OptionalStringArrayProperty -Object $CaseEntry -PropertyName 'extra_cache'
        Profile = Get-OptionalStringProperty -Object $subjectEntry -PropertyName 'profile'
        Board = Get-OptionalStringProperty -Object $subjectEntry -PropertyName 'board'
        ActiveFacets = Get-OptionalStringArrayProperty -Object $subjectEntry -PropertyName 'active_facets'
    }
}

function Get-ExportCases {
    if (-not (Test-Path $exportCaseManifestPath)) {
        throw "export case manifest not found: $exportCaseManifestPath"
    }

    $manifestData = Get-Content -LiteralPath $exportCaseManifestPath -Raw -Encoding utf8 | ConvertFrom-Json
    $schemaName = Get-RequiredStringProperty -Object $manifestData -PropertyName 'schema' -Context $exportCaseManifestPath
    if ($schemaName -ne 'materialized_graph.export_case_manifest/v1') {
        throw "unsupported export case manifest schema '$schemaName' in $exportCaseManifestPath"
    }

    $rawCases = Get-ObjectPropertyValue -Object $manifestData -PropertyName 'cases'
    if ($null -eq $rawCases) {
        throw "missing 'cases' array in $exportCaseManifestPath"
    }

    $cases = @()
    $seenNames = @{}
    $caseEntries = @($rawCases)
    for ($index = 0; $index -lt $caseEntries.Count; ++$index) {
        $entry = Convert-ExportCaseEntry -CaseEntry $caseEntries[$index] -ManifestPath $exportCaseManifestPath -Index $index
        if ($seenNames.ContainsKey($entry.Name)) {
            throw "duplicate export case name '$($entry.Name)' in $exportCaseManifestPath"
        }

        $seenNames[$entry.Name] = $true
        $cases += $entry
    }

    return $cases
}

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
    if ([string]::IsNullOrWhiteSpace($baseFull) -or [string]::IsNullOrWhiteSpace($targetFull)) {
        throw "relative path resolution requires non-empty base and target"
    }
    $trimChars = [char[]]@('\', '/')
    $baseUri = New-Object System.Uri(($baseFull.TrimEnd($trimChars) + [System.IO.Path]::DirectorySeparatorChar))
    $targetUri = New-Object System.Uri($targetFull)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Get-GraphSummary {
    param(
        [string]$JsonPath
    )

    if ([string]::IsNullOrWhiteSpace($JsonPath) -or -not (Test-Path $JsonPath)) {
        return $null
    }

    $graph = Get-Content -LiteralPath $JsonPath -Raw -Encoding utf8 | ConvertFrom-Json
    Assert-MaterializedGraphSampleShape -Graph $graph -Context $JsonPath
    $kindCounts = [ordered]@{}
    foreach ($node in $graph.nodes) {
        $kind = [string]$node.kind
        if ([string]::IsNullOrWhiteSpace($kind)) {
            $kind = 'unknown'
        }

        if ($kindCounts.Contains($kind)) {
            $kindCounts[$kind] += 1
        } else {
            $kindCounts[$kind] = 1
        }
    }

    $summary = [ordered]@{
        schema = $graph.schema
        node_count = $graph.node_count
        edge_count = $graph.edge_count
        effective_max_phase = $graph.effective_max_phase
        effective_runlevel_mask = $graph.effective_runlevel_mask
        effective_runlevel_text = $graph.effective_runlevel_text
    }
    if ($kindCounts.Count -gt 0) {
        $summary.node_kinds = $kindCounts
    }

    return $summary
}

function New-CaseSubjectMetadata {
    param(
        $Entry
    )

    $profile = $null
    if ($null -ne $Entry.Profile -and -not [string]::IsNullOrWhiteSpace([string]$Entry.Profile)) {
        $profile = [string]$Entry.Profile
    }

    $board = $null
    if ($null -ne $Entry.Board -and -not [string]::IsNullOrWhiteSpace([string]$Entry.Board)) {
        $board = [string]$Entry.Board
    }

    $activeFacets = @()
    if ($null -ne $Entry.ActiveFacets) {
        $activeFacets = @(
            @($Entry.ActiveFacets) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ }
        )
    }

    return [ordered]@{
        profile = $profile
        board = $board
        active_facets = $activeFacets
    }
}

function Write-ExportBundleIndex {
    param(
        [object[]]$Results,
        [string]$OutputRootPath
    )

    $bundleRoot = Resolve-FullPath $OutputRootPath
    if (-not (Test-Path $bundleRoot)) {
        New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null
    }

    $cases = @()
    foreach ($result in @($Results)) {
        if ($null -eq $result -or $null -eq $result.PSObject.Properties['DotPath'] -or $null -eq $result.PSObject.Properties['JsonPath']) {
            continue
        }

        $caseEntry = [ordered]@{
            name = $result.Name
            source = $result.Source
            build_dir = $result.BuildDir
            build_target = $result.BuildTarget
            export_target = $result.ExportTarget
            dot = Get-RelativePath -BasePath $bundleRoot -TargetPath $result.DotPath
            json = Get-RelativePath -BasePath $bundleRoot -TargetPath $result.JsonPath
        }

        if ($null -ne $result.PSObject.Properties['Subject'] -and $null -ne $result.Subject) {
            $caseEntry.subject = $result.Subject
        }

        $summary = Get-GraphSummary -JsonPath $result.JsonPath
        if ($null -ne $summary) {
            $caseEntry.graph = $summary
        }

        $cases += $caseEntry
    }

    $index = [ordered]@{
        schema = 'materialized_graph.export_bundle/v1'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        case_count = $cases.Count
        cases = $cases
    }

    $indexPath = Join-Path $bundleRoot 'index.json'
    $index | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $indexPath -Encoding utf8
    Write-Host "[INDEX] $indexPath"
    return $indexPath
}

function Invoke-LegacyExport {
    $sourceDir = Join-Path $repoRoot $Source
    $buildDir = Join-Path $repoRoot $BuildDir

    if (-not (Test-Path $sourceDir)) {
        throw "source not found: $sourceDir"
    }

    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "[CFG] $sourceDir"
    cmake -S $sourceDir -B $buildDir -G Ninja -D CMAKE_C_COMPILER=$CCompiler -D CMAKE_CXX_COMPILER=$CxxCompiler
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed"
    }

    if ($ConfigureOnly) {
        Write-Host "[OK] configure finished: $buildDir"
        return
    }

    if ([string]::IsNullOrWhiteSpace($Dot) -and [string]::IsNullOrWhiteSpace($Json)) {
        Write-Host "[EXPORT] target=$ExportTarget"
        cmake --build $buildDir --target $ExportTarget -j $Jobs
        if ($LASTEXITCODE -ne 0) {
            throw "export target failed"
        }

        Write-Host "[OK] exported via target"
        Write-Host "[DOT]  $(Join-Path $buildDir 'materialized_graph.dot')"
        Write-Host "[JSON] $(Join-Path $buildDir 'materialized_graph.sample.json')"
        return
    }

    Write-Host "[BUILD] target=$BuildTarget"
    cmake --build $buildDir --target $BuildTarget -j $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "build failed"
    }

    $exePath = Join-Path $buildDir "$BuildTarget.exe"
    if (-not (Test-Path $exePath)) {
        throw "executable not found: $exePath"
    }

    $args = @()
    if (-not [string]::IsNullOrWhiteSpace($Dot)) {
        $args += @("--dot", $Dot)
    }
    if (-not [string]::IsNullOrWhiteSpace($Json)) {
        $args += @("--json", $Json)
    }

    Write-Host "[RUN] $exePath $($args -join ' ')"
    & $exePath @args
    if ($LASTEXITCODE -ne 0) {
        throw "export run failed"
    }

    Write-Host "[OK] exported via direct run"
}

function Invoke-ManifestCase {
    param(
        [hashtable]$Entry,
        [string]$DotOverride = '',
        [string]$JsonOverride = '',
        [string]$OutputRootPath = ''
    )

    $sourceDir = Join-Path $repoRoot $Entry.Source
    $buildDir = Join-Path $repoRoot $Entry.BuildDir
    if (-not (Test-Path $sourceDir)) {
        throw "source not found: $sourceDir"
    }

    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    $dotPath = ''
    $jsonPath = ''
    if (-not [string]::IsNullOrWhiteSpace($OutputRootPath)) {
        $caseOutputDir = Join-Path (Resolve-FullPath $OutputRootPath) $Entry.Name
        New-Item -ItemType Directory -Path $caseOutputDir -Force | Out-Null
        $dotPath = Join-Path $caseOutputDir $Entry.DefaultDot
        $jsonPath = Join-Path $caseOutputDir $Entry.DefaultJson
    } else {
        $dotPath = if ([string]::IsNullOrWhiteSpace($DotOverride)) {
            Join-Path $buildDir $Entry.DefaultDot
        } else {
            Resolve-FullPath $DotOverride
        }
        $jsonPath = if ([string]::IsNullOrWhiteSpace($JsonOverride)) {
            Join-Path $buildDir $Entry.DefaultJson
        } else {
            Resolve-FullPath $JsonOverride
        }
    }

    Ensure-ParentDirectory -Path $dotPath
    Ensure-ParentDirectory -Path $jsonPath

    $configureArgs = @(
        '-S', $sourceDir,
        '-B', $buildDir,
        '-G', 'Ninja',
        '-D', "CMAKE_C_COMPILER=$CCompiler",
        '-D', "CMAKE_CXX_COMPILER=$CxxCompiler"
    )
    if (-not [string]::IsNullOrWhiteSpace($Entry.DotCache)) {
        $configureArgs += @('-D', "$($Entry.DotCache)=$dotPath")
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.JsonCache)) {
        $configureArgs += @('-D', "$($Entry.JsonCache)=$jsonPath")
    }
    foreach ($cacheArg in $Entry.ExtraCache) {
        $configureArgs += @('-D', $cacheArg)
    }

    Write-Host "[CFG][$($Entry.Name)] $sourceDir"
    & cmake @configureArgs | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed: $($Entry.Name)"
    }

    if ($ConfigureOnly) {
        Write-Host "[OK][$($Entry.Name)] configure finished: $buildDir"
        return [pscustomobject]@{
            Name = $Entry.Name
            Source = $Entry.Source
            BuildDir = $Entry.BuildDir
            BuildTarget = $Entry.BuildTarget
            ExportTarget = $Entry.ExportTarget
            DotPath = $dotPath
            JsonPath = $jsonPath
            Subject = New-CaseSubjectMetadata -Entry $Entry
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.ExportTarget)) {
        Write-Host "[EXPORT][$($Entry.Name)] target=$($Entry.ExportTarget)"
        cmake --build $buildDir --target $Entry.ExportTarget -j $Jobs | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "export target failed: $($Entry.Name)"
        }
    } else {
        Write-Host "[BUILD][$($Entry.Name)] target=$($Entry.BuildTarget)"
        cmake --build $buildDir --target $Entry.BuildTarget -j $Jobs | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "build failed: $($Entry.Name)"
        }
    }

    Write-Host "[DOT][$($Entry.Name)]  $dotPath"
    Write-Host "[JSON][$($Entry.Name)] $jsonPath"
    return [pscustomobject]@{
        Name = $Entry.Name
        Source = $Entry.Source
        BuildDir = $Entry.BuildDir
        BuildTarget = $Entry.BuildTarget
        ExportTarget = $Entry.ExportTarget
        DotPath = $dotPath
        JsonPath = $jsonPath
        Subject = New-CaseSubjectMetadata -Entry $Entry
    }
}

$manifestCases = Get-ExportCases

if ($ListCases) {
    foreach ($entry in $manifestCases) {
        $subject = New-CaseSubjectMetadata -Entry $entry
        $subjectParts = @()
        if (-not [string]::IsNullOrWhiteSpace([string]$subject.profile)) {
            $subjectParts += "profile=$([string]$subject.profile)"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$subject.board)) {
            $subjectParts += "board=$([string]$subject.board)"
        }
        if (@($subject.active_facets).Count -gt 0) {
            $subjectParts += "facets=$((@($subject.active_facets) -join ','))"
        }

        $line = "$($entry.Name) -> source=$($entry.Source) target=$($entry.ExportTarget)"
        if ($subjectParts.Count -gt 0) {
            $line += " subject={$($subjectParts -join '; ')}"
        }

        Write-Host $line
    }
    exit 0
}

$useManifest = $AllCases -or $Case.Count -gt 0

if (-not [string]::IsNullOrWhiteSpace($OutputRoot) -and -not $useManifest) {
    throw "-OutputRoot requires -Case or -AllCases"
}

if (-not [string]::IsNullOrWhiteSpace($OutputRoot) -and (-not [string]::IsNullOrWhiteSpace($Dot) -or -not [string]::IsNullOrWhiteSpace($Json))) {
    throw "-OutputRoot cannot be combined with -Dot/-Json"
}

if (-not $useManifest) {
    Invoke-LegacyExport
    exit 0
}

if (($AllCases -or $Case.Count -gt 1) -and (-not [string]::IsNullOrWhiteSpace($Dot) -or -not [string]::IsNullOrWhiteSpace($Json))) {
    throw "-Dot/-Json can only be used with a single selected case"
}

$selected = @()
if ($AllCases) {
    $selected = $manifestCases
} else {
    foreach ($caseName in $Case) {
        $match = $manifestCases | Where-Object { $_.Name -eq $caseName }
        if (-not $match) {
            throw "unknown case: $caseName"
        }
        $selected += $match
    }
}

$results = @()
foreach ($entry in $selected) {
    $dotOverride = if ($selected.Count -eq 1) { $Dot } else { '' }
    $jsonOverride = if ($selected.Count -eq 1) { $Json } else { '' }
    $results += Invoke-ManifestCase -Entry $entry -DotOverride $dotOverride -JsonOverride $jsonOverride -OutputRootPath $OutputRoot
}

$indexPath = ''
if (-not $ConfigureOnly -and -not [string]::IsNullOrWhiteSpace($OutputRoot)) {
    $indexPath = Write-ExportBundleIndex -Results $results -OutputRootPath $OutputRoot
}

if ($ConfigureOnly) {
    Write-Host '[OK] configure finished for selected cases'
} else {
    Write-Host '[OK] materialized graph export finished for selected cases'
    if (-not [string]::IsNullOrWhiteSpace($indexPath)) {
        Write-Host "[OK] export bundle index written: $indexPath"
    }
}
