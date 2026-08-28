[CmdletBinding()]
param(
    [string]$RepoRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $scriptPath = $MyInvocation.MyCommand.Path
    $RepoRoot = Split-Path -Parent (Split-Path -Parent $scriptPath)
}

function Fail([string]$Message) {
    throw "[charm-core-governance] $Message"
}

function TextFromCodePoints([int[]]$CodePoints) {
    return -join ($CodePoints | ForEach-Object { [char]$_ })
}

$root = (Resolve-Path -LiteralPath $RepoRoot).Path
$canonical = @(
    'CONSTITUTION.md',
    'README.md',
    'docs/README.md',
    'docs/architecture/README.md',
    'docs/architecture/charm_core_contract.md',
    'docs/agent/routes/architecture.md',
    'docs/agent/routes/capability.md',
    'docs/agent/routes/docs.md'
)

foreach ($relative in $canonical) {
    $path = Join-Path $root $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Fail "canonical file missing: $relative"
    }
}

$position = TextFromCodePoints @(
    67, 104, 97, 114, 109, 32, 26159, 19968, 20010, 33021, 21147,
    23548, 21521, 30340, 23884, 20837, 24335, 24212, 29992, 24179, 21488
)
$positionFiles = @(
    'README.md',
    'docs/README.md',
    'docs/architecture/README.md',
    'docs/architecture/charm_core_contract.md'
)
foreach ($relative in $positionFiles) {
    $content = Get-Content -Raw -Encoding utf8 (Join-Path $root $relative)
    if (-not $content.Contains($position)) {
        Fail "position drift: $relative"
    }
}

$licenseName = Get-Content -Encoding utf8 (Join-Path $root 'LICENSE') -TotalCount 1
if ($licenseName -ne 'Apache License') {
    Fail "license drift: LICENSE"
}
$rootReadme = Get-Content -Raw -Encoding utf8 (Join-Path $root 'README.md')
$licenseBadge = '[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-green.svg?style=flat-square)](LICENSE)'
if (-not $rootReadme.Contains($licenseBadge)) {
    Fail "license badge drift: README.md"
}

$legacyClaims = @(
    (TextFromCodePoints @(22810, 25112, 32447, 27597, 20179)),
    (TextFromCodePoints @(24403, 21069, 26368, 37325, 35201, 30340, 26041, 27861, 35770)),
    (TextFromCodePoints @(24403, 21069, 26368, 37325, 35201, 30340, 24179, 21488, 21270, 20027, 32447)),
    (TextFromCodePoints @(21807, 19968, 20027, 32447)),
    'RTE -> H747',
    'Capability -> Component -> Profile -> Projection -> Evidence'
)
foreach ($relative in $canonical) {
    $content = Get-Content -Raw -Encoding utf8 (Join-Path $root $relative)
    foreach ($claim in $legacyClaims) {
        if ($content.Contains($claim)) {
            Fail "legacy claim '$claim' in canonical file: $relative"
        }
    }
}

$constitution = Get-Content -Raw -Encoding utf8 (Join-Path $root 'CONSTITUTION.md')
$verdictConcepts = @(
    'Capability Contract',
    'Requirement',
    'Provision',
    'Binding',
    'ResolvedBinding / BindingSnapshot',
    'Provider',
    'Component',
    'Profile',
    'Backend',
    'Driver',
    'Compiler',
    'Graph',
    'Evidence'
)
foreach ($concept in $verdictConcepts) {
    if (-not $constitution.Contains("| $concept |")) {
        Fail "first verdict missing: $concept"
    }
}

$coreHeaderPath = Join-Path $root 'Modules/core/capability/relations.hpp'
$coreHeader = Get-Content -Raw -Encoding utf8 $coreHeaderPath
$publicStructs = @(
    [regex]::Matches($coreHeader, '\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)') |
        ForEach-Object { $_.Groups[1].Value }
)
$expectedPublicStructs = @('Requirement', 'Provision', 'Binding')
if (($publicStructs -join ',') -ne ($expectedPublicStructs -join ',')) {
    Fail "Core public struct set drift: $($publicStructs -join ', ')"
}
$retiredPublicTokens = @(
    'ResolvedBinding',
    'ResolutionFailure'
)
foreach ($token in $retiredPublicTokens) {
    if ($coreHeader.Contains($token)) {
        Fail "retired Core public token present: $token"
    }
}

$architectureRoot = Join-Path $root 'docs/architecture'
$canonicalArchitecture = @()
foreach ($file in Get-ChildItem -LiteralPath $architectureRoot -Filter '*.md' -File) {
    $head = (Get-Content -Encoding utf8 $file.FullName -TotalCount 20) -join "`n"
    if ($head.Contains('`status`: `canonical`')) {
        $canonicalArchitecture += $file.Name
    }
}
if ($canonicalArchitecture.Count -ne 1 -or
    $canonicalArchitecture[0] -ne 'charm_core_contract.md') {
    Fail "canonical architecture set drift: $($canonicalArchitecture -join ', ')"
}

$expectedStatuses = @{
    'docs/overview.md' = 'supporting'
    'docs/architecture_overview.md' = 'supporting'
}
foreach ($entry in $expectedStatuses.GetEnumerator()) {
    $path = Join-Path $root $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Fail "classified document missing: $($entry.Key)"
    }
    $head = (Get-Content -Encoding utf8 $path -TotalCount 20) -join "`n"
    if (-not $head.Contains("``$($entry.Value)``")) {
        Fail "document status drift: $($entry.Key) expected=$($entry.Value)"
    }
}

$rootCmake = Get-Content -Raw -Encoding utf8 (Join-Path $root 'CMakeLists.txt')
$requiredBuildSurface = @(
    'add_library(Charm-core INTERFACE)',
    'target_compile_features(Charm-core INTERFACE cxx_std_26)',
    'add_library(Charm::core ALIAS Charm-core)',
    'EXPORT_NAME core',
    'install(EXPORT CharmCoreTargets',
    'FILE CharmCoreConfig.cmake'
)
foreach ($token in $requiredBuildSurface) {
    if (-not $rootCmake.Contains($token)) {
        Fail "Core build surface missing: $token"
    }
}
$retiredBuildSurface = @(
    'Charm::os',
    'Charm-build-config',
    'CHARM_ENABLE_TEST_MODULES',
    'CHARM_TARGET_HAS_HOSTED_CXX',
    'CHARM_TARGET_HAS_CXX_MATH'
)
foreach ($token in $retiredBuildSurface) {
    if ($rootCmake.Contains($token)) {
        Fail "retired build surface present: $token"
    }
}

$consumerFiles = @(
    'Examples/system/charm_capability_relations/CMakeLists.txt',
    'Examples/system/charm_capability_mvp/CMakeLists.txt',
    'Examples/system/charm_capability_mvp/qemu/CMakeLists.txt',
    'Examples/system/charm_core_external_consumer/CMakeLists.txt',
    'Examples/system/charm_core_arm_build_only/CMakeLists.txt'
)
foreach ($relative in $consumerFiles) {
    $content = Get-Content -Raw -Encoding utf8 (Join-Path $root $relative)
    if (-not $content.Contains('Charm::core')) {
        Fail "Core consumer does not link Charm::core: $relative"
    }
}
$externalConsumer = Get-Content -Raw -Encoding utf8 (
    Join-Path $root 'Examples/system/charm_core_external_consumer/CMakeLists.txt')
if (-not $externalConsumer.Contains('find_package(CharmCore CONFIG REQUIRED)') -or
    $externalConsumer.Contains('add_subdirectory')) {
    Fail 'installed Core consumer boundary drift'
}

$relationCmake = Get-Content -Raw -Encoding utf8 (
    Join-Path $root 'Examples/system/charm_capability_relations/CMakeLists.txt')
$negativeRelationCases = @(
    'same_requirement_key',
    'same_provision_key',
    'same_binding_key'
)
if (-not $relationCmake.Contains('charm-capability-relations-negative-${case_name}')) {
    Fail 'Core relation negative target template missing'
}
foreach ($case in $negativeRelationCases) {
    $relative = "Examples/system/charm_capability_relations/negative_${case}.cpp"
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) {
        Fail "Core relation negative case missing: $case"
    }
    if (-not $relationCmake.Contains($case)) {
        Fail "Core relation negative case not wired: $case"
    }
}

$portableVerifier = Get-Content -Raw -Encoding utf8 (
    Join-Path $root 'Examples/system/charm_capability_mvp/verify_portable_source_boundary.ps1')
if (-not $portableVerifier.Contains('[ValidateSet("host", "qemu")]') -or
    -not $portableVerifier.Contains('qemu = Join-Path $PSScriptRoot "qemu/main.cpp"')) {
    Fail 'MVP portable consumer domains drift'
}

$tracked = @(& git -C $root -c core.quotepath=false ls-files)
if ($LASTEXITCODE -ne 0) {
    Fail 'cannot enumerate tracked files'
}
$allowedRootFiles = @(
    '.gitattributes',
    '.gitignore',
    'AGENTS.md',
    'CMakeLists.txt',
    'CMakePresets.json',
    'CONSTITUTION.md',
    'Examples/README.md',
    'docs/README.md',
    'docs/architecture_overview.md',
    'docs/documentation_maintenance.md',
    'docs/overview.md',
    'Examples/system/README.md',
    'LICENSE',
    'README.md',
    'config.toml',
    '.github/PULL_REQUEST_TEMPLATE.md',
    '.github/workflows/onlycore-host.yml',
    'Modules/core/capability/relations.hpp',
    'scripts/check_charm_core_governance.ps1'
)
$allowedPathPrefixes = @(
    'docs/agent/',
    'docs/architecture/',
    'docs/project/',
    'Examples/system/charm_capability_mvp/',
    'Examples/system/charm_capability_relations/',
    'Examples/system/charm_core_arm_build_only/',
    'Examples/system/charm_core_external_consumer/'
)
foreach ($relative in $tracked) {
    if ($allowedRootFiles -contains $relative) {
        continue
    }
    $allowed = $false
    foreach ($prefix in $allowedPathPrefixes) {
        if ($relative.StartsWith($prefix, [StringComparison]::Ordinal)) {
            $allowed = $true
            break
        }
    }
    if (-not $allowed) {
        Fail "path outside OnlyCore allowlist: $relative"
    }
}

$linkFiles = $canonical + @(
    'docs/architecture/charm_core_semantic_audit.md',
    'docs/overview.md',
    'docs/architecture_overview.md'
)
$linkCount = 0
foreach ($relative in $linkFiles) {
    $path = Join-Path $root $relative
    $content = Get-Content -Raw -Encoding utf8 $path
    foreach ($match in [regex]::Matches($content, '!?(?:\[[^\]]*\])\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim()
        if ($target.StartsWith('<') -and $target.EndsWith('>')) {
            $target = $target.Substring(1, $target.Length - 2)
        }
        if ($target -match '^(https?:|mailto:|#)') {
            continue
        }
        $target = ($target -split '#', 2)[0]
        if ([string]::IsNullOrWhiteSpace($target)) {
            continue
        }
        try {
            $target = [Uri]::UnescapeDataString($target)
        } catch {
            Fail "invalid link escape in ${relative}: $target"
        }
        $resolved = Join-Path (Split-Path -Parent $path) $target
        if (-not (Test-Path -LiteralPath $resolved)) {
            Fail "local link missing in ${relative}: $target"
        }
        ++$linkCount
    }
}

Write-Output "[charm-core-governance] canonical=ok files=$($canonical.Count)"
Write-Output "[charm-core-governance] verdicts=ok count=$($verdictConcepts.Count)"
Write-Output "[charm-core-governance] public-relations=ok count=$($expectedPublicStructs.Count)"
Write-Output "[charm-core-governance] classified=ok count=$($expectedStatuses.Count)"
Write-Output "[charm-core-governance] build-surface=ok consumers=$($consumerFiles.Count)"
Write-Output "[charm-core-governance] allowlist=ok prefixes=$($allowedPathPrefixes.Count) root_files=$($allowedRootFiles.Count)"
Write-Output "[charm-core-governance] links=ok count=$linkCount"
Write-Output '[charm-core-governance] ok'
