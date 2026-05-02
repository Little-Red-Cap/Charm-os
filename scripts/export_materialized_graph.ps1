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
    [string]$OutputRoot = "",
    [string]$CaseManifest = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$defaultExportCaseManifestPath = Join-Path $PSScriptRoot 'materialized_graph.export_case_manifest.v1.json'
. (Join-Path $PSScriptRoot 'materialized_graph_schema.ps1')

function Resolve-CaseManifestPath {
    if (-not [string]::IsNullOrWhiteSpace($CaseManifest)) {
        return Resolve-FullPath $CaseManifest
    }

    return Resolve-FullPath $defaultExportCaseManifestPath
}

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

function Get-CaseKindProperty {
    param(
        $Object,
        [string]$Context
    )

    $value = Get-ObjectPropertyValue -Object $Object -PropertyName 'case_kind'
    if ([string]::IsNullOrWhiteSpace([string]$value)) {
        return 'materialized_graph'
    }

    $normalized = [string]$value
    if ($normalized -notin @('materialized_graph', 'runtime_only', 'fact_only')) {
        throw "unsupported case_kind '$normalized' in $Context"
    }

    return $normalized
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

function Get-OptionalDeclaredContractsProperty {
    param(
        $Object,
        [string]$PropertyName,
        [string]$Context
    )

    $value = Get-ObjectPropertyValue -Object $Object -PropertyName $PropertyName
    if ($null -eq $value) {
        return @()
    }

    $contracts = @()
    $entries = @($value)
    for ($index = 0; $index -lt $entries.Count; ++$index) {
        $entry = $entries[$index]
        $entryContext = "$Context $PropertyName[$index]"
        $contractName = Get-RequiredStringProperty -Object $entry -PropertyName 'contract' -Context $entryContext
        $requires = Get-OptionalStringArrayProperty -Object $entry -PropertyName 'requires'
        $contracts += [pscustomobject]@{
            contract = $contractName
            requires = @($requires | Sort-Object -Unique)
        }
    }

    return @($contracts)
}

function Assert-ExportCaseEntrySemantics {
    param(
        $Entry,
        [string]$Context
    )

    if ($Entry.CaseKind -eq 'materialized_graph') {
        if ([string]::IsNullOrWhiteSpace($Entry.ExportTarget)) {
            throw "materialized_graph case requires export_target in $Context"
        }
        if ([string]::IsNullOrWhiteSpace($Entry.DefaultDot)) {
            throw "materialized_graph case requires default_dot in $Context"
        }
        if ([string]::IsNullOrWhiteSpace($Entry.DefaultJson)) {
            throw "materialized_graph case requires default_json in $Context"
        }
        return
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.ExportTarget)) {
        throw "$($Entry.CaseKind) case must not declare export_target in $Context"
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.DotCache)) {
        throw "$($Entry.CaseKind) case must not declare dot_cache in $Context"
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.JsonCache)) {
        throw "$($Entry.CaseKind) case must not declare json_cache in $Context"
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.DefaultDot)) {
        throw "$($Entry.CaseKind) case must not declare default_dot in $Context"
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.DefaultJson)) {
        throw "$($Entry.CaseKind) case must not declare default_json in $Context"
    }

    if ($Entry.CaseKind -eq 'fact_only') {
        return
    }

    if ([string]::IsNullOrWhiteSpace($Entry.RuntimeObserveTarget) -and
        [string]::IsNullOrWhiteSpace($Entry.RuntimeObserve) -and
        [string]::IsNullOrWhiteSpace($Entry.DefaultRuntimeObserve)) {
        throw "runtime_only case requires runtime observe output in $Context"
    }
}

function Convert-ExportCaseEntry {
    param(
        $CaseEntry,
        [string]$ManifestPath,
        [int]$Index
    )

    $context = "$ManifestPath cases[$Index]"
    $subjectEntry = Get-ObjectPropertyValue -Object $CaseEntry -PropertyName 'subject'

    $entry = [pscustomobject]@{
        Name = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'name' -Context $context
        Source = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'source' -Context $context
        BuildDir = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'build_dir' -Context $context
        BuildTarget = Get-RequiredStringProperty -Object $CaseEntry -PropertyName 'build_target' -Context $context
        CaseKind = Get-CaseKindProperty -Object $CaseEntry -Context $context
        ExportTarget = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'export_target'
        DotCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'dot_cache'
        JsonCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'json_cache'
        DefaultDot = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'default_dot'
        DefaultJson = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'default_json'
        RuntimeObserveTarget = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'runtime_observe_target'
        RuntimeObserveCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'runtime_observe_cache'
        DefaultRuntimeObserve = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'default_runtime_observe'
        FactEvidenceTarget = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'fact_evidence_target'
        FactEvidenceCache = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'fact_evidence_cache'
        DefaultFactEvidence = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'default_fact_evidence'
        FactEvidence = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'fact_evidence'
        ExtraCache = Get-OptionalStringArrayProperty -Object $CaseEntry -PropertyName 'extra_cache'
        RuntimeObserve = Get-OptionalStringProperty -Object $CaseEntry -PropertyName 'runtime_observe'
        Profile = Get-OptionalStringProperty -Object $subjectEntry -PropertyName 'profile'
        Board = Get-OptionalStringProperty -Object $subjectEntry -PropertyName 'board'
        ActiveFacets = Get-OptionalStringArrayProperty -Object $subjectEntry -PropertyName 'active_facets'
        DeclaredFacts = Get-OptionalStringArrayProperty -Object $CaseEntry -PropertyName 'declared_facts'
        RequiredFacts = Get-OptionalStringArrayProperty -Object $CaseEntry -PropertyName 'required_facts'
        AuditProvidedFacts = Get-OptionalStringArrayProperty -Object $CaseEntry -PropertyName 'audit_provided_facts'
        DeclaredContracts = Get-OptionalDeclaredContractsProperty -Object $CaseEntry -PropertyName 'declared_contracts' -Context $context
    }

    Assert-ExportCaseEntrySemantics -Entry $entry -Context $context
    return $entry
}

function Get-ExportCases {
    $resolvedManifestPath = Resolve-CaseManifestPath
    if (-not (Test-Path $resolvedManifestPath)) {
        throw "export case manifest not found: $resolvedManifestPath"
    }

    $manifestData = Get-Content -LiteralPath $resolvedManifestPath -Raw -Encoding utf8 | ConvertFrom-Json
    $schemaName = Get-RequiredStringProperty -Object $manifestData -PropertyName 'schema' -Context $resolvedManifestPath
    if ($schemaName -ne 'materialized_graph.export_case_manifest/v1') {
        throw "unsupported export case manifest schema '$schemaName' in $resolvedManifestPath"
    }

    $rawCases = Get-ObjectPropertyValue -Object $manifestData -PropertyName 'cases'
    if ($null -eq $rawCases) {
        throw "missing 'cases' array in $resolvedManifestPath"
    }

    $cases = @()
    $seenNames = @{}
    $caseEntries = @($rawCases)
    for ($index = 0; $index -lt $caseEntries.Count; ++$index) {
        $entry = Convert-ExportCaseEntry -CaseEntry $caseEntries[$index] -ManifestPath $resolvedManifestPath -Index $index
        if ($seenNames.ContainsKey($entry.Name)) {
            throw "duplicate export case name '$($entry.Name)' in $resolvedManifestPath"
        }

        $seenNames[$entry.Name] = $true
        $cases += $entry
    }

    return [pscustomobject]@{
        Path = $resolvedManifestPath
        Schema = $schemaName
        Cases = @($cases)
    }
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

function Resolve-OptionalExistingPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $resolvedPath = Resolve-FullPath $Path
    if (Test-Path $resolvedPath) {
        return $resolvedPath
    }

    return $null
}

function Resolve-RuntimeObserveOutputPath {
    param(
        $Entry,
        [string]$BuildDirPath,
        [string]$OutputRootPath = ''
    )

    if ($null -eq $Entry) {
        return ''
    }

    if (-not [string]::IsNullOrWhiteSpace($OutputRootPath) -and
        -not [string]::IsNullOrWhiteSpace($Entry.DefaultRuntimeObserve)) {
        $caseOutputDir = Join-Path (Resolve-FullPath $OutputRootPath) $Entry.Name
        return Join-Path $caseOutputDir $Entry.DefaultRuntimeObserve
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.RuntimeObserve)) {
        return Resolve-FullPath $Entry.RuntimeObserve
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.DefaultRuntimeObserve)) {
        return Join-Path $BuildDirPath $Entry.DefaultRuntimeObserve
    }

    return ''
}

function Resolve-FactEvidenceOutputPath {
    param(
        $Entry,
        [string]$BuildDirPath,
        [string]$OutputRootPath = ''
    )

    if ($null -eq $Entry) {
        return ''
    }

    if (-not [string]::IsNullOrWhiteSpace($OutputRootPath) -and
        -not [string]::IsNullOrWhiteSpace($Entry.DefaultFactEvidence)) {
        $caseOutputDir = Join-Path (Resolve-FullPath $OutputRootPath) $Entry.Name
        return Join-Path $caseOutputDir $Entry.DefaultFactEvidence
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidence)) {
        return Resolve-FullPath $Entry.FactEvidence
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.DefaultFactEvidence)) {
        return Join-Path $BuildDirPath $Entry.DefaultFactEvidence
    }

    return ''
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

function Get-OptionalRelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    if ([string]::IsNullOrWhiteSpace($TargetPath)) {
        return $null
    }

    return Get-RelativePath -BasePath $BasePath -TargetPath $TargetPath
}

function Get-FactEvidenceFacts {
    param(
        $FactEvidence,
        [string]$PropertyName
    )

    if ($null -eq $FactEvidence) {
        return @()
    }

    $source = $FactEvidence
    if ($null -ne $FactEvidence.PSObject.Properties['facts'] -and $null -ne $FactEvidence.facts) {
        $source = $FactEvidence.facts
    }

    if ($null -eq $source.PSObject.Properties[$PropertyName]) {
        return @()
    }

    return @(
        @($source.$PropertyName) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function Load-FactEvidence {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $resolvedPath = Resolve-FullPath $Path
    if (-not (Test-Path $resolvedPath)) {
        return $null
    }

    $evidence = Get-Content -LiteralPath $resolvedPath -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$evidence.schema -ne 'system_compiler.fact_evidence/v0') {
        throw "unsupported fact evidence schema: $([string]$evidence.schema)"
    }

    return $evidence
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
    $connectionModeCounts = [ordered]@{}
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

        if ($null -ne $node.PSObject.Properties['connection'] -and $null -ne $node.connection) {
            $mode = [string]$node.connection.mode
            if (-not [string]::IsNullOrWhiteSpace($mode)) {
                if ($connectionModeCounts.Contains($mode)) {
                    $connectionModeCounts[$mode] += 1
                } else {
                    $connectionModeCounts[$mode] = 1
                }
            }
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
    if ($connectionModeCounts.Count -gt 0) {
        $summary.connection_modes = $connectionModeCounts
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

function New-CaseDeclaredFactsMetadata {
    param(
        $Entry,
        $FactEvidence = $null
    )

    $declaredFacts = @()
    if ($null -ne $Entry -and $null -ne $Entry.PSObject.Properties['DeclaredFacts']) {
        $declaredFacts = @(
            @($Entry.DeclaredFacts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Select-Object -Unique
        )
    }

    return @(
        @($declaredFacts) +
        @(Get-FactEvidenceFacts -FactEvidence $FactEvidence -PropertyName 'declared_facts') |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function New-CaseRequiredFactsMetadata {
    param(
        $Entry,
        $FactEvidence = $null
    )

    $requiredFacts = @()
    if ($null -ne $Entry -and $null -ne $Entry.PSObject.Properties['RequiredFacts']) {
        $requiredFacts = @(
            @($Entry.RequiredFacts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Select-Object -Unique
        )
    }

    return @(
        @($requiredFacts) +
        @(Get-FactEvidenceFacts -FactEvidence $FactEvidence -PropertyName 'required_facts') |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function New-CaseAuditProvidedFactsMetadata {
    param(
        $Entry,
        $FactEvidence = $null
    )

    $auditProvidedFacts = @()
    if ($null -ne $Entry -and $null -ne $Entry.PSObject.Properties['AuditProvidedFacts']) {
        $auditProvidedFacts = @(
            @($Entry.AuditProvidedFacts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Select-Object -Unique
        )
    }

    return @(
        @($auditProvidedFacts) +
        @(Get-FactEvidenceFacts -FactEvidence $FactEvidence -PropertyName 'audit_provided_facts') |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function New-CaseDeclaredContractsMetadata {
    param(
        $Entry
    )

    if ($null -eq $Entry -or $null -eq $Entry.PSObject.Properties['DeclaredContracts']) {
        return @()
    }

    $normalized = @()
    foreach ($contract in @($Entry.DeclaredContracts)) {
        if ($null -eq $contract) {
            continue
        }

        $contractName = [string]$contract.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $requires = @()
        if ($null -ne $contract.PSObject.Properties['requires']) {
            $requires = @(
                @($contract.requires) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ } |
                    Sort-Object -Unique
            )
        }

        $normalized += [ordered]@{
            contract = $contractName
            requires = @($requires)
        }
    }

    return @($normalized)
}

function Write-ExportBundleIndex {
    param(
        [object[]]$Results,
        [string]$OutputRootPath,
        $CaseManifestInfo = $null
    )

    $bundleRoot = Resolve-FullPath $OutputRootPath
    if (-not (Test-Path $bundleRoot)) {
        New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null
    }

    $cases = @()
    foreach ($result in @($Results)) {
        if ($null -eq $result) {
            continue
        }

        $caseEntry = [ordered]@{
            name = $result.Name
            source = $result.Source
            build_dir = $result.BuildDir
            build_target = $result.BuildTarget
            case_kind = $result.CaseKind
            export_target = $result.ExportTarget
            dot = Get-OptionalRelativePath -BasePath $bundleRoot -TargetPath $result.DotPath
            json = Get-OptionalRelativePath -BasePath $bundleRoot -TargetPath $result.JsonPath
        }

        if ($null -ne $result.PSObject.Properties['Subject'] -and $null -ne $result.Subject) {
            $caseEntry.subject = $result.Subject
        }
        if ($null -ne $result.PSObject.Properties['DeclaredFacts']) {
            $caseEntry.declared_facts = @($result.DeclaredFacts)
        }
        if ($null -ne $result.PSObject.Properties['RequiredFacts']) {
            $caseEntry.required_facts = @($result.RequiredFacts)
        }
        if ($null -ne $result.PSObject.Properties['AuditProvidedFacts']) {
            $caseEntry.audit_provided_facts = @($result.AuditProvidedFacts)
        }
        if ($null -ne $result.PSObject.Properties['FactEvidencePath'] -and -not [string]::IsNullOrWhiteSpace([string]$result.FactEvidencePath)) {
            $caseEntry.fact_evidence = Get-OptionalRelativePath -BasePath $bundleRoot -TargetPath $result.FactEvidencePath
        } elseif ($null -ne $result.PSObject.Properties['FactEvidence'] -and -not [string]::IsNullOrWhiteSpace([string]$result.FactEvidence)) {
            Write-Warning "[INDEX][$($result.Name)] fact evidence artifact not found: $([string]$result.FactEvidence)"
        }
        if ($null -ne $result.PSObject.Properties['DeclaredContracts']) {
            $caseEntry.declared_contracts = @($result.DeclaredContracts)
        }
        if ($null -ne $result.PSObject.Properties['RuntimeObservePath'] -and -not [string]::IsNullOrWhiteSpace([string]$result.RuntimeObservePath)) {
            $caseEntry.runtime_observe = Get-OptionalRelativePath -BasePath $bundleRoot -TargetPath $result.RuntimeObservePath
        } elseif ($null -ne $result.PSObject.Properties['RuntimeObserve'] -and -not [string]::IsNullOrWhiteSpace([string]$result.RuntimeObserve)) {
            Write-Warning "[INDEX][$($result.Name)] runtime observe artifact not found: $([string]$result.RuntimeObserve)"
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
    if ($null -ne $CaseManifestInfo) {
        $index.input_manifest = [ordered]@{
            path = [string]$CaseManifestInfo.Path
            schema = [string]$CaseManifestInfo.Schema
        }
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
        $Entry,
        [string]$DotOverride = '',
        [string]$JsonOverride = '',
        [string]$OutputRootPath = ''
    )

    $sourceDir = Join-Path $repoRoot $Entry.Source
    $buildDir = Join-Path $repoRoot $Entry.BuildDir
    $hasMaterializedGraph = $Entry.CaseKind -eq 'materialized_graph'
    if (-not (Test-Path $sourceDir)) {
        throw "source not found: $sourceDir"
    }

    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    $dotPath = $null
    $jsonPath = $null
    if ($hasMaterializedGraph) {
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
    }

    $runtimeObservePath = Resolve-RuntimeObserveOutputPath -Entry $Entry -BuildDirPath $buildDir -OutputRootPath $OutputRootPath
    if (-not [string]::IsNullOrWhiteSpace($Entry.RuntimeObserveTarget) -and
        [string]::IsNullOrWhiteSpace($runtimeObservePath)) {
        throw "runtime observe target requires a resolved runtime observe path: $($Entry.Name)"
    }
    $factEvidencePath = Resolve-FactEvidenceOutputPath -Entry $Entry -BuildDirPath $buildDir -OutputRootPath $OutputRootPath
    if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidenceTarget) -and
        [string]::IsNullOrWhiteSpace($factEvidencePath)) {
        throw "fact evidence target requires a resolved fact evidence path: $($Entry.Name)"
    }

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
    if (-not [string]::IsNullOrWhiteSpace($Entry.RuntimeObserveCache) -and
        -not [string]::IsNullOrWhiteSpace($runtimeObservePath)) {
        Ensure-ParentDirectory -Path $runtimeObservePath
        $configureArgs += @('-D', "$($Entry.RuntimeObserveCache)=$runtimeObservePath")
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidenceCache) -and
        -not [string]::IsNullOrWhiteSpace($factEvidencePath)) {
        Ensure-ParentDirectory -Path $factEvidencePath
        $configureArgs += @('-D', "$($Entry.FactEvidenceCache)=$factEvidencePath")
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
        $factEvidenceInfo = Load-FactEvidence -Path $factEvidencePath
        Write-Host "[OK][$($Entry.Name)] configure finished: $buildDir"
        return [pscustomobject]@{
            Name = $Entry.Name
            Source = $Entry.Source
            BuildDir = $Entry.BuildDir
            BuildTarget = $Entry.BuildTarget
            CaseKind = $Entry.CaseKind
            ExportTarget = $Entry.ExportTarget
            DotPath = $dotPath
            JsonPath = $jsonPath
            Subject = New-CaseSubjectMetadata -Entry $Entry
            DeclaredFacts = New-CaseDeclaredFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
            RequiredFacts = New-CaseRequiredFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
            AuditProvidedFacts = New-CaseAuditProvidedFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
            DeclaredContracts = New-CaseDeclaredContractsMetadata -Entry $Entry
            RuntimeObserve = $Entry.RuntimeObserve
            RuntimeObservePath = $runtimeObservePath
            FactEvidence = $Entry.FactEvidence
            FactEvidencePath = Resolve-OptionalExistingPath -Path $factEvidencePath
        }
    }

    if ($Entry.CaseKind -eq 'fact_only') {
        Write-Host "[BUILD][$($Entry.Name)] target=$($Entry.BuildTarget)"
        cmake --build $buildDir --target $Entry.BuildTarget -j $Jobs | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "build failed: $($Entry.Name)"
        }
    } elseif (-not [string]::IsNullOrWhiteSpace($Entry.ExportTarget)) {
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

    if (-not [string]::IsNullOrWhiteSpace($Entry.RuntimeObserveTarget)) {
        Write-Host "[RUNTIME_OBSERVE][$($Entry.Name)] target=$($Entry.RuntimeObserveTarget)"
        cmake --build $buildDir --target $Entry.RuntimeObserveTarget -j $Jobs | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "runtime observe target failed: $($Entry.Name)"
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidenceTarget)) {
        Write-Host "[FACT_EVIDENCE][$($Entry.Name)] target=$($Entry.FactEvidenceTarget)"
        cmake --build $buildDir --target $Entry.FactEvidenceTarget -j $Jobs | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "fact evidence target failed: $($Entry.Name)"
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($dotPath)) {
        Write-Host "[DOT][$($Entry.Name)]  $dotPath"
    }
    if (-not [string]::IsNullOrWhiteSpace($jsonPath)) {
        Write-Host "[JSON][$($Entry.Name)] $jsonPath"
    }
    $runtimeObserveArtifactPath = Resolve-OptionalExistingPath -Path $runtimeObservePath
    $factEvidenceArtifactPath = Resolve-OptionalExistingPath -Path $factEvidencePath
    if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidenceTarget) -and $null -eq $factEvidenceArtifactPath) {
        throw "fact evidence target did not produce expected artifact: $factEvidencePath"
    }
    $factEvidenceInfo = Load-FactEvidence -Path $factEvidenceArtifactPath
    return [pscustomobject]@{
        Name = $Entry.Name
        Source = $Entry.Source
        BuildDir = $Entry.BuildDir
        BuildTarget = $Entry.BuildTarget
        CaseKind = $Entry.CaseKind
        ExportTarget = $Entry.ExportTarget
        DotPath = $dotPath
        JsonPath = $jsonPath
        Subject = New-CaseSubjectMetadata -Entry $Entry
        DeclaredFacts = New-CaseDeclaredFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
        RequiredFacts = New-CaseRequiredFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
        AuditProvidedFacts = New-CaseAuditProvidedFactsMetadata -Entry $Entry -FactEvidence $factEvidenceInfo
        DeclaredContracts = New-CaseDeclaredContractsMetadata -Entry $Entry
        RuntimeObserve = if (-not [string]::IsNullOrWhiteSpace($Entry.RuntimeObserve)) {
            $Entry.RuntimeObserve
        } else {
            $runtimeObservePath
        }
        RuntimeObservePath = $runtimeObserveArtifactPath
        FactEvidence = if (-not [string]::IsNullOrWhiteSpace($Entry.FactEvidence)) {
            $Entry.FactEvidence
        } else {
            $factEvidencePath
        }
        FactEvidencePath = $factEvidenceArtifactPath
    }
}

$useManifest = $AllCases -or $Case.Count -gt 0
$exportCaseManifest = $null
$manifestCases = @()

if (-not $ListCases -and -not $useManifest -and -not [string]::IsNullOrWhiteSpace($CaseManifest)) {
    throw "-CaseManifest requires -Case, -AllCases, or -ListCases"
}

if ($ListCases -or $useManifest) {
    $exportCaseManifest = Get-ExportCases
    $manifestCases = @($exportCaseManifest.Cases)
}

if ($ListCases) {
    Write-Host "[MANIFEST] $($exportCaseManifest.Path)"
    foreach ($entry in $manifestCases) {
        $subject = New-CaseSubjectMetadata -Entry $entry
        $declaredFacts = New-CaseDeclaredFactsMetadata -Entry $entry
        $requiredFacts = New-CaseRequiredFactsMetadata -Entry $entry
        $auditProvidedFacts = New-CaseAuditProvidedFactsMetadata -Entry $entry
        $declaredContracts = New-CaseDeclaredContractsMetadata -Entry $entry
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

        $exportTargetText = if ([string]::IsNullOrWhiteSpace([string]$entry.ExportTarget)) { '<none>' } else { [string]$entry.ExportTarget }
        $line = "$($entry.Name) -> kind=$($entry.CaseKind) source=$($entry.Source) target=$exportTargetText"
        if ($subjectParts.Count -gt 0) {
            $line += " subject={$($subjectParts -join '; ')}"
        }
        if (@($declaredFacts).Count -gt 0) {
            $line += " declared_facts={$((@($declaredFacts) -join ', '))}"
        }
        if (@($requiredFacts).Count -gt 0) {
            $line += " required_facts={$((@($requiredFacts) -join ', '))}"
        }
        if (@($auditProvidedFacts).Count -gt 0) {
            $line += " audit_provided_facts={$((@($auditProvidedFacts) -join ', '))}"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.FactEvidenceTarget)) {
            $line += " fact_evidence_target={$([string]$entry.FactEvidenceTarget)}"
        } elseif (-not [string]::IsNullOrWhiteSpace([string]$entry.FactEvidence)) {
            $line += " fact_evidence={$([string]$entry.FactEvidence)}"
        } elseif (-not [string]::IsNullOrWhiteSpace([string]$entry.DefaultFactEvidence)) {
            $line += " default_fact_evidence={$([string]$entry.DefaultFactEvidence)}"
        }
        if (@($declaredContracts).Count -gt 0) {
            $contractText = @(
                $declaredContracts |
                    ForEach-Object {
                        "$([string]$_.contract) requires [$((@($_.requires) -join ', '))]"
                    }
            ) -join '; '
            $line += " declared_contracts={$contractText}"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.RuntimeObserve)) {
            $line += " runtime_observe={$([string]$entry.RuntimeObserve)}"
        }

        Write-Host $line
    }
    exit 0
}

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
    $indexPath = Write-ExportBundleIndex -Results $results -OutputRootPath $OutputRoot -CaseManifestInfo $exportCaseManifest
}

if ($ConfigureOnly) {
    Write-Host '[OK] configure finished for selected cases'
} else {
    Write-Host '[OK] materialized graph export finished for selected cases'
    if (-not [string]::IsNullOrWhiteSpace($indexPath)) {
        Write-Host "[OK] export bundle index written: $indexPath"
    }
}
