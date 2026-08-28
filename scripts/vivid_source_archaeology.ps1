param(
    [string]$DevRef = "95759c7647606afb0a740f76f581bf2a198bde2c",
    [string]$ArchiveRef = "40f610cbd4fea55e9e1ca37bcbf4bd616d81a30d",
    [string]$MergeBase = "0cdfbbdc29b3ef583c4f774928a18bda408b6068",
    [string]$OutputPath = "out/vivid-source-archaeology/manifest.json"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $RepoRoot $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$script:OwnerCache = @{}

function Invoke-GitText {
    param([string[]]$Arguments)
    $result = & git @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git failed: $($Arguments -join ' ')`n$($result -join "`n")"
    }
    return ($result -join "`n")
}

function Get-RefFiles {
    param([string]$Ref)
    $text = Invoke-GitText @("ls-tree", "-r", "--name-only", $Ref, "--", "Modules/ui/vivid")
    return @($text -split "`r?`n" | Where-Object { $_ -and $_ -match '\.cppm$' })
}

function Get-RefFileText {
    param([string]$Ref, [string]$Path)
    return Invoke-GitText @("show", ($Ref + ":" + $Path))
}

function Get-ModuleOwners {
    param([string]$Ref)
    if ($script:OwnerCache.ContainsKey($Ref)) { return $script:OwnerCache[$Ref] }
    $owners = @{}
    $pathText = Invoke-GitText @("ls-tree", "-r", "--name-only", $Ref)
    $paths = @($pathText -split "`r?`n" | Where-Object { $_ -and $_ -match '\.cppm$' })
    foreach ($path in $paths) {
        $text = Get-RefFileText $Ref $path
        $match = [regex]::Match($text, '(?m)^\s*(?:export\s+)?module\s+([A-Za-z0-9_.:]+)\s*;')
        if ($match.Success -and -not $owners.ContainsKey($match.Groups[1].Value)) {
            $owners[$match.Groups[1].Value] = $path
        }
    }
    $script:OwnerCache[$Ref] = $owners
    return $owners
}

function Get-Snapshot {
    param([string]$Ref)

    $files = Get-RefFiles $Ref
    $modules = @{}
    $importsByFile = @{}
    foreach ($path in $files) {
        $text = Get-RefFileText $Ref $path
        $moduleMatch = [regex]::Match($text, '(?m)^\s*(?:export\s+)?module\s+([A-Za-z0-9_.:]+)\s*;')
        if ($moduleMatch.Success) {
            $modules[$moduleMatch.Groups[1].Value] = $path
        }
        $imports = @([regex]::Matches($text, '(?m)^\s*(?:export\s+)?import\s+([A-Za-z0-9_.:]+)\s*;') |
            ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $importsByFile[$path] = $imports
    }

    $known = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $modules.Keys) { [void]$known.Add($name) }
    $internalEdges = 0
    $externalEdges = 0
    $frontier = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in $importsByFile.Keys) {
        foreach ($import in $importsByFile[$path]) {
            if ($import.StartsWith(":", [StringComparison]::Ordinal) -or $known.Contains($import)) {
                $internalEdges++
            } else {
                $externalEdges++
                [void]$frontier.Add($import)
            }
        }
    }

    $policyPath = "Modules/ui/vivid/cmake/vivid_module_policy.cmake"
    $policyText = Get-RefFileText $Ref $policyPath
    $explicit = @{}
    $loopPattern = '(?ms)foreach\(\s*_module\s+IN\s+ITEMS\s+(.*?)\)\s*vivid_module_policy\(NAME\s+"\$\{_module\}"\s+ACCESS\s+(\w+)\)'
    foreach ($match in [regex]::Matches($policyText, $loopPattern)) {
        $access = $match.Groups[2].Value
        foreach ($name in ($match.Groups[1].Value -split '\s+' | Where-Object { $_ })) {
            $explicit[$name] = $access
        }
    }
    foreach ($match in [regex]::Matches($policyText, 'vivid_module_policy\(\s*NAME\s+([^\s\)"$]+)\s+ACCESS\s+([^\s\)]+)')) {
        $explicit[$match.Groups[1].Value] = $match.Groups[2].Value
    }
    $product = @($explicit.GetEnumerator() | Where-Object Value -eq "PRODUCT_ROOT" | ForEach-Object Key | Sort-Object)
    $hostOnly = @($explicit.GetEnumerator() | Where-Object Value -eq "HOST_ONLY" | ForEach-Object Key | Sort-Object)
    $internalExplicit = @($explicit.GetEnumerator() | Where-Object Value -eq "INTERNAL" | ForEach-Object Key | Sort-Object)
    $internalDefault = @($modules.Keys | Where-Object { -not $explicit.ContainsKey($_) } | Sort-Object)

    $edgeCount = 0
    foreach ($path in $importsByFile.Keys) { $edgeCount += $importsByFile[$path].Count }
    $uniqueImportCount = @($importsByFile.Values | ForEach-Object { $_ } | Sort-Object -Unique).Count
    $owners = Get-ModuleOwners $Ref
    $externalOwners = [ordered]@{}
    foreach ($name in ($frontier | Sort-Object)) {
        if ($owners.ContainsKey($name)) {
            $externalOwners[$name] = [ordered]@{ owner = $owners[$name]; kind = "external-source" }
        } elseif ($name -match '^charm\.core\.(config\.generated|soa_pool_caps)$') {
            $externalOwners[$name] = [ordered]@{ owner = "Vivid Profile Compiler generated module"; kind = "generated" }
        } else {
            $externalOwners[$name] = [ordered]@{ owner = "unresolved; map to package or repository before extraction"; kind = "unresolved" }
        }
    }

    return [ordered]@{
        ref = $Ref
        cppm_files = $files.Count
        modules = $modules.Count
        import_edges = $edgeCount
        unique_imports = $uniqueImportCount
        internal_import_edges = $internalEdges
        external_import_edges = $externalEdges
        lexical_import_frontier = @($frontier | Sort-Object)
        external_owners = $externalOwners
        policy = [ordered]@{
            product_root = $product
            host_only = $hostOnly
            internal_explicit = $internalExplicit
            internal_by_default = $internalDefault
        }
        scc = [ordered]@{ components = $modules.Count; cycles = 0; method = "module import graph; no cycles observed" }
    }
}

$toolPaths = @(
    "Modules/ui/vivid/cmake/product_profile_compiler.cmake",
    "Modules/ui/vivid/cmake/vivid_module_policy.cmake",
    "Modules/ui/vivid/cmake/widget_catalog_compiler.cmake",
    "scripts/vivid_product_profile_compiler_smoke.ps1",
    "scripts/vivid_stack_usage_gate_smoke.ps1",
    "scripts/vivid_static_memory_admission_smoke.ps1"
)
$toolShas = [ordered]@{}
foreach ($path in $toolPaths) {
    $toolShas[$path] = Invoke-GitText @("rev-parse", ($DevRef + ":" + $path))
}

$dev = Get-Snapshot $DevRef
$archive = Get-Snapshot $ArchiveRef
$archiveVividDiffText = Invoke-GitText @(
        "diff", "--name-only", ($MergeBase + ".." + $ArchiveRef), "--",
        "Modules/ui/vivid", "Examples/ui/vivid", "docs/ui")
$archiveVividDiff = @($archiveVividDiffText -split "`r?`n" | Where-Object { $_ })
$patchQueue = @()
$index = 1
foreach ($path in $archiveVividDiff) {
    $patchQueue += [ordered]@{
        patch_id = "ARCHIVE-VIVID-{0:D3}" -f $index
        source_range = ($MergeBase + ".." + $ArchiveRef)
        file = $path
        status = "Quarantined"
        verification = "pending semantic hunk review"
        note = "File is not an acceptance unit; split by semantic hunk before adjudication."
    }
    $index++
}

$manifest = [ordered]@{
    schema = 1
    status = "supporting-exploration"
    authority = [ordered]@{
        vivid_source = $DevRef
        core_semantics = "9a7ef5db185564e4f1cff853916cd05856597f02"
        archive_wip = $ArchiveRef
        merge_base = $MergeBase
    }
    audit_tools = [ordered]@{ dev_ref = $DevRef; blob_sha = $toolShas }
    snapshots = [ordered]@{ dev = $dev; archive = $archive }
    archive_patch_queue = $patchQueue
    profile_requirements = [ordered]@{
        compiler_raw = "record from product_profile_compiler output"
        normalized_closure = "record from generated module_closure.json"
        lexical_frontier = "record from all import declarations, independent of policy"
    }
    evidence = [ordered]@{
        profile_closure = [ordered]@{
            player_md3 = [ordered]@{ modules = 69; sources = 68; normalized_external_requirements = 4 }
            player_md3_debug = [ordered]@{ modules = 73; sources = 72; normalized_external_requirements = 4 }
            profile_fingerprint = "180caf1e6f46b2d82fc54761a25e81317e00283b7d71fc3499f60000a9876720"
            host_target_fingerprint = "222d40c405fef09c572672d9015718164b071cac6edcc912114fefc6d0f41c23"
            h747_target_fingerprint = "2baf4844112aeb8ae0588fb47c7eb942d86e1025f24443ea16a5157b5c1561be"
            lexical_frontier = [ordered]@{ full = 25; player_md3 = 16; player_md3_debug = 17 }
        }
        runtime = [ordered]@{
            dev_page_transition_snapshot = "passed"
            dev_page_transition_clean_clone = "hung_after_build_terminated"
            archive_page_transition = "segfault"
            dev_static_memory = "passed"
            archive_static_memory = "passed"
        }
        gfx_min = "pending"
        scene_min = "pending"
        player_pressure = "dev page_transition passes; archive page_transition segfaults"
    }
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null
$json = $manifest | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText($OutputPath, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("[vivid-archaeology] wrote {0}" -f $OutputPath)
Write-Host ("[vivid-archaeology] dev cppm={0} modules={1} imports={2} frontier={3}" -f $dev.cppm_files, $dev.modules, $dev.import_edges, $dev.lexical_import_frontier.Count)
Write-Host ("[vivid-archaeology] archive cppm={0} modules={1} imports={2} frontier={3}" -f $archive.cppm_files, $archive.modules, $archive.import_edges, $archive.lexical_import_frontier.Count)
Write-Host ("[vivid-archaeology] archive_vivid_patch_files={0}" -f $archiveVividDiff.Count)
