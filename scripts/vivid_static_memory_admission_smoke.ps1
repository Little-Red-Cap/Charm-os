param(
    [string]$BuildDir = "Examples/ui/vivid/soa_demo/cmake-build-soa-ci",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$repoPrefix = $RepoRoot.TrimEnd('\') + '\'
if (-not $BuildDir.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir must stay inside the repository: $BuildDir"
}

$SoaSourceDir = Join-Path $RepoRoot "Examples/ui/vivid/soa_demo"
$ProductFixtureDir = Join-Path $BuildDir "vivid-product-admission-source"
$fixturePrefix = $BuildDir.TrimEnd('\') + '\'
if (-not $ProductFixtureDir.StartsWith($fixturePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Product fixture escaped the build directory: $ProductFixtureDir"
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Invoke-VividConfigure {
    param(
        [string]$SourceDir,
        [string]$FeatureSet,
        [string[]]$ExtraArgs = @(),
        [bool]$ExpectSuccess = $true
    )

    $cmakeArgs = @(
        "--fresh",
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", $Generator,
        "-DCHARM_VIVID_FEATURESET=$FeatureSet"
    ) + $ExtraArgs
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $CMakeExe @cmakeArgs 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }

    $success = $exitCode -eq 0
    if ($success -ne $ExpectSuccess) {
        $output | Out-Host
        throw "Unexpected configure result for ${FeatureSet}: success=$success expected=$ExpectSuccess"
    }
    return ($output -join "`n")
}

function Get-GeneratedDir {
    param(
        [string]$Profile
    )

    return Join-Path $BuildDir "Charm/generated/vivid/Charm-ui/$Profile"
}

function Read-KeyValueManifest {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing Vivid manifest: $Path"
    }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        if ($line -match '^([^=]+)=(.*)$') {
            $values[$Matches[1]] = $Matches[2]
        }
    }
    return $values
}

function Read-JsonFile {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing Vivid JSON evidence: $Path"
    }
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Assert-Admission {
    param(
        [hashtable]$Values,
        [string]$FeatureSet,
        [string]$Profile,
        [string]$Status,
        [int64]$MinimumHeadroom
    )

    if ($Values.featureset -ne $FeatureSet -or
        $Values.profile -ne $Profile -or
        $Values.status -ne $Status) {
        throw "Unexpected manifest identity: featureset=$($Values.featureset) profile=$($Values.profile) status=$($Values.status)"
    }
    $upper = [int64]$Values.upper_bound_bytes
    $budget = [int64]$Values.budget_bytes
    $headroom = [int64]$Values.configured_headroom_bytes
    if ($upper -le 0) {
        throw "Upper bound must be positive"
    }
    foreach ($field in @(
        "command_buffer_upper_bytes",
        "draw_cmd_compaction_workspace_upper_bytes",
        "draw_cmd_executor_workspace_upper_bytes",
        "soa_traversal_workspace_upper_bytes",
        "runtime_globals_upper_bytes"
    )) {
        if ([int64]$Values[$field] -le 0) {
            throw "Static memory manifest field must be positive: $field=$($Values[$field])"
        }
    }
    if ($Values.draw_cmd_max_commands -ne "1024" -or
        $Values.draw_cmd_text_bytes -ne "4096" -or
        $Values.draw_cmd_blob_bytes -ne "2048") {
        throw "Unexpected DrawCmd profile in static memory manifest"
    }
    if ($Values.max_hot_stack_frame_bytes -ne "4096") {
        throw "Unexpected Vivid stack frame limit: $($Values.max_hot_stack_frame_bytes)"
    }
    if ($Values.style_patch_slot_cap -ne "192" -or
        $Values.style_patch_pool_upper_bytes -ne "49152") {
        throw "Unexpected sparse StylePatch budget in static memory manifest"
    }
    if ($Status -eq "admitted" -and
        ($budget -le 0 -or $headroom -lt $MinimumHeadroom)) {
        throw "Admission headroom is insufficient: upper=$upper budget=$budget headroom=$headroom"
    }
    Write-Host "[vivid-static-memory] featureset=$FeatureSet profile=$Profile status=$Status upper=$upper budget=$budget headroom=$headroom"
}

function Assert-ProductEvidence {
    param(
        [string]$Profile,
        [bool]$DebugProfile
    )

    $generatedDir = Get-GeneratedDir -Profile $Profile
    $profileEvidence = Read-JsonFile -Path (Join-Path $generatedDir "profile.json")
    $envelopeEvidence = Read-JsonFile -Path (Join-Path $generatedDir "target_envelope.json")
    $closureEvidence = Read-JsonFile -Path (Join-Path $generatedDir "module_closure.json")
    $admissionEvidence = Read-JsonFile -Path (Join-Path $generatedDir "admission.json")
    $manifest = Read-KeyValueManifest -Path (Join-Path $generatedDir "static_memory_admission.txt")

    Assert-Admission -Values $manifest -FeatureSet "PRODUCT" -Profile $Profile -Status "admitted" -MinimumHeadroom 524288
    if ([int64]$profileEvidence.workset.style_patch_slot_cap -ne 192 -or
        [int64]$admissionEvidence.static_memory.style_patch_pool_upper_bytes -ne 49152) {
        throw "PRODUCT sparse StylePatch evidence mismatch for profile '$Profile'"
    }
    if ($profileEvidence.profile_fingerprint -ne $envelopeEvidence.profile_fingerprint -or
        $profileEvidence.profile_fingerprint -ne $closureEvidence.profile_fingerprint -or
        $profileEvidence.profile_fingerprint -ne $admissionEvidence.profile_fingerprint -or
        $profileEvidence.profile_fingerprint -ne $manifest.profile_fingerprint) {
        throw "PRODUCT evidence fingerprint mismatch for profile '$Profile'"
    }
    if ($envelopeEvidence.target_fingerprint -ne $closureEvidence.target_fingerprint -or
        $envelopeEvidence.target_fingerprint -ne $admissionEvidence.target_fingerprint -or
        $envelopeEvidence.target_fingerprint -ne $manifest.target_fingerprint) {
        throw "PRODUCT target fingerprint mismatch for profile '$Profile'"
    }

    $expectedSceneKindCount = 15
    $expectedObjectKindCount = if ($DebugProfile) { 2 } else { 0 }
    if (@($profileEvidence.widget_kinds).Count -ne $expectedSceneKindCount -or
        @($profileEvidence.object_widget_kinds).Count -ne $expectedObjectKindCount) {
        throw "Unexpected widget profile for '$Profile': scene=$(@($profileEvidence.widget_kinds).Count) object=$(@($profileEvidence.object_widget_kinds).Count)"
    }
    foreach ($expected in @(
        "charm.gfx.color",
        "charm.core.soa_kernel:semantic",
        "charm.core.soa_kernel:storage"
    )) {
        if (@($closureEvidence.modules) -notcontains $expected) {
            throw "PRODUCT closure '$Profile' is missing '$expected'"
        }
    }
    foreach ($forbidden in @(
        "charm.gfx.snapshot",
        "charm.gfx.host_tools",
        "charm.ui.scene.motion_runtime",
        "charm.ui.scene.page_transition"
    )) {
        if (@($closureEvidence.modules) -contains $forbidden) {
            throw "PRODUCT closure '$Profile' contains forbidden module '$forbidden'"
        }
    }
    $expectedExternalRequirements = @(
        "charm.core.event",
        "charm.font",
        "charm.font.provider_vfs",
        "charm.font.typography"
    )
    $actualExternalRequirements = @($closureEvidence.external_requirements)
    if ($actualExternalRequirements.Count -ne $expectedExternalRequirements.Count) {
        throw "PRODUCT closure '$Profile' external requirement count drifted: $($actualExternalRequirements.Count)"
    }
    foreach ($expected in $expectedExternalRequirements) {
        if ($actualExternalRequirements -notcontains $expected) {
            throw "PRODUCT closure '$Profile' is missing external requirement '$expected'"
        }
    }

    $stackSourcesPath = Join-Path $generatedDir "stack_usage_sources.txt"
    if (-not (Test-Path -LiteralPath $stackSourcesPath)) {
        throw "Missing PRODUCT stack source manifest: $stackSourcesPath"
    }
    $stackSources = @(Get-Content -LiteralPath $stackSourcesPath -Encoding UTF8)
    foreach ($expected in @(
        "Modules/ui/vivid/charm.ui.vivid.cppm",
        "Modules/ui/vivid/core/style_sheet.cppm",
        "Modules/ui/vivid/gfx/svg.cppm"
    )) {
        if ($stackSources -notcontains $expected) {
            throw "PRODUCT stack source manifest '$Profile' is missing '$expected'"
        }
    }
    if ($stackSources -contains "Modules/ui/vivid/gfx/snapshot.cppm") {
        throw "PRODUCT stack source manifest '$Profile' contains host-only snapshot"
    }

    $tableSource = "Modules/ui/vivid/widgets/table_view.cppm"
    $treeSource = "Modules/ui/vivid/widgets/tree_view.cppm"
    if ($DebugProfile) {
        if ($profileEvidence.object_widget_kinds -notcontains "TableView" -or
            $profileEvidence.object_widget_kinds -notcontains "TreeView" -or
            $stackSources -notcontains $tableSource -or
            $stackSources -notcontains $treeSource) {
            throw "Debug profile did not admit TableView/TreeView object modules"
        }
    } elseif ($stackSources -contains $tableSource -or $stackSources -contains $treeSource) {
        throw "Base PRODUCT profile was polluted by debug-only widget modules"
    }

    Write-Host "[vivid-static-memory] product_profile=$Profile modules=$(@($closureEvidence.modules).Count) stack_sources=$($stackSources.Count) fingerprint=$($profileEvidence.profile_fingerprint)"
    return $profileEvidence.profile_fingerprint
}

$ProductFixture = @'
cmake_minimum_required(VERSION 4.0)
project(vivid-product-admission-smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if(NOT DEFINED CHARM_ROOT OR NOT DEFINED SMOKE_STATIC_MEMORY_BUDGET_BYTES)
    message(FATAL_ERROR "CHARM_ROOT and SMOKE_STATIC_MEMORY_BUDGET_BYTES are required")
endif()
file(TO_CMAKE_PATH "${CHARM_ROOT}" CHARM_ROOT)
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

set(CHARM_BUILD_FULL_RUNTIME ON CACHE BOOL "" FORCE)
set(CHARM_BUILD_AUDIO_COMPONENT OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)

include("${CHARM_ROOT}/Examples/project/player/cmake/player_md3_vivid_product.cmake")
vivid_configure_product_target(
    TARGET Charm-ui
    PROFILE "${CHARM_PLAYER_VIVID_PROFILE}"
    SCREEN_WIDTH 568
    SCREEN_HEIGHT 1210
    PIXEL_FORMAT RGB888
    LAYER_CACHE_SLOTS 2
    LAYER_CACHE_WIDTH 568
    LAYER_CACHE_HEIGHT 1210
    RUNTIME_SCENE_INSTANCES 1
    STATIC_MEMORY_BUDGET_BYTES "${SMOKE_STATIC_MEMORY_BUDGET_BYTES}"
    STATIC_MEMORY_MIN_HEADROOM_BYTES 524288
    MAX_HOT_STACK_FRAME_BYTES 4096)

add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
'@

$CommonSoaArgs = @(
    "-DCHARM_VIVID_SOA_MAX_NODES=256",
    "-DCHARM_VIVID_SOA_TEXT_ARENA_BYTES=",
    "-DCHARM_VIVID_STYLE_PATCH_SLOT_CAP=192",
    "-DCHARM_VIVID_STYLE_CLASS_MAX=256",
    "-DCHARM_VIVID_STYLE_RULE_CAP=32",
    "-DCHARM_VIVID_STYLE_METRICS_POOL_CAP=64",
    "-DCHARM_VIVID_SCREEN_WIDTH=800",
    "-DCHARM_VIVID_SCREEN_HEIGHT=480",
    "-DCHARM_VIVID_SCREEN_PIXEL_FORMAT=RGB888",
    "-DCHARM_VIVID_LAYER_CACHE_SLOTS=1",
    "-DCHARM_VIVID_LAYER_CACHE_WIDTH=400",
    "-DCHARM_VIVID_LAYER_CACHE_HEIGHT=240"
)
$FullArgs = $CommonSoaArgs + @(
    "-DCHARM_VIVID_RUNTIME_SCENE_INSTANCES=1",
    "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=",
    "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES="
)
$McuArgs = $CommonSoaArgs + @(
    "-DCHARM_VIVID_RUNTIME_SCENE_INSTANCES=1",
    "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=1835008",
    "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES=262144"
)
$ProductBaseArgs = @(
    "-DCHARM_ROOT=$($RepoRoot.Replace('\', '/'))",
    "-DCHARM_PLAYER_DEBUG_UI=OFF",
    "-DSMOKE_STATIC_MEMORY_BUDGET_BYTES=6291456"
)
$ProductDebugArgs = @(
    "-DCHARM_ROOT=$($RepoRoot.Replace('\', '/'))",
    "-DCHARM_PLAYER_DEBUG_UI=ON",
    "-DSMOKE_STATIC_MEMORY_BUDGET_BYTES=6291456"
)

try {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    if (Test-Path -LiteralPath $ProductFixtureDir) {
        Remove-Item -LiteralPath $ProductFixtureDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $ProductFixtureDir -Force | Out-Null
    Write-Utf8NoBom -Path (Join-Path $ProductFixtureDir "CMakeLists.txt") -Content $ProductFixture

    Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "FULL" -ExtraArgs $FullArgs | Out-Null
    $fullManifest = Join-Path (Get-GeneratedDir -Profile "full") "static_memory_admission.txt"
    Assert-Admission -Values (Read-KeyValueManifest -Path $fullManifest) -FeatureSet "FULL" -Profile "full" -Status "profile_only" -MinimumHeadroom 0

    $missingSceneArgs = $CommonSoaArgs + @(
        "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=1835008",
        "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES=262144"
    )
    $missingScene = Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "MCU_MIN" -ExtraArgs $missingSceneArgs -ExpectSuccess $false
    if ($missingScene -notmatch 'CHARM_VIVID_RUNTIME_SCENE_INSTANCES') {
        throw "MCU_MIN missing-scene-count failure did not report the expected rule"
    }

    $missingBudgetArgs = $CommonSoaArgs + @(
        "-DCHARM_VIVID_RUNTIME_SCENE_INSTANCES=1",
        "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=",
        "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES="
    )
    $missingBudget = Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "MCU_MIN" -ExtraArgs $missingBudgetArgs -ExpectSuccess $false
    if ($missingBudget -notmatch 'CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES') {
        throw "MCU_MIN missing-budget failure did not report the expected rule"
    }

    $zeroHeadroomArgs = $CommonSoaArgs + @(
        "-DCHARM_VIVID_RUNTIME_SCENE_INSTANCES=1",
        "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=1835008",
        "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES=0"
    )
    $zeroHeadroom = Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "MCU_MIN" -ExtraArgs $zeroHeadroomArgs -ExpectSuccess $false
    if ($zeroHeadroom -notmatch 'CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES must be > 0') {
        throw "MCU_MIN zero-headroom failure did not report the expected rule"
    }

    Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "MCU_MIN" -ExtraArgs $McuArgs | Out-Null
    $mcuGeneratedDir = Get-GeneratedDir -Profile "mcu_min"
    $mcuManifest = Join-Path $mcuGeneratedDir "static_memory_admission.txt"
    Assert-Admission -Values (Read-KeyValueManifest -Path $mcuManifest) -FeatureSet "MCU_MIN" -Profile "mcu_min" -Status "admitted" -MinimumHeadroom 262144
    $mcuStackSources = @(Get-Content -LiteralPath (Join-Path $mcuGeneratedDir "stack_usage_sources.txt") -Encoding UTF8)
    if ($mcuStackSources -contains "Modules/ui/vivid/core/perf_overlay_runtime.cppm") {
        throw "MCU_MIN source manifest contains PerfOverlay runtime"
    }

    Invoke-VividConfigure -SourceDir $ProductFixtureDir -FeatureSet "PRODUCT" -ExtraArgs $ProductBaseArgs | Out-Null
    $baseFingerprint = Assert-ProductEvidence -Profile "player_md3" -DebugProfile $false

    $tooSmallArgs = @(
        "-DCHARM_ROOT=$($RepoRoot.Replace('\', '/'))",
        "-DCHARM_PLAYER_DEBUG_UI=OFF",
        "-DSMOKE_STATIC_MEMORY_BUDGET_BYTES=3145728"
    )
    $tooSmall = Invoke-VividConfigure -SourceDir $ProductFixtureDir -FeatureSet "PRODUCT" -ExtraArgs $tooSmallArgs -ExpectSuccess $false
    if ($tooSmall -notmatch 'Vivid static memory admission failed') {
        throw "PRODUCT insufficient-budget failure did not report the expected rule"
    }

    Invoke-VividConfigure -SourceDir $ProductFixtureDir -FeatureSet "PRODUCT" -ExtraArgs $ProductDebugArgs | Out-Null
    $debugFingerprint = Assert-ProductEvidence -Profile "player_md3_debug" -DebugProfile $true
    if ($debugFingerprint -eq $baseFingerprint) {
        throw "Base and debug profiles must have different fingerprints"
    }

    Invoke-VividConfigure -SourceDir $ProductFixtureDir -FeatureSet "PRODUCT" -ExtraArgs $ProductBaseArgs | Out-Null
    $restoredFingerprint = Assert-ProductEvidence -Profile "player_md3" -DebugProfile $false
    if ($restoredFingerprint -ne $baseFingerprint) {
        throw "Base PRODUCT fingerprint changed after debug profile switch"
    }
} finally {
    Invoke-VividConfigure -SourceDir $SoaSourceDir -FeatureSet "FULL" -ExtraArgs $FullArgs | Out-Null
    if (Test-Path -LiteralPath $ProductFixtureDir) {
        Remove-Item -LiteralPath $ProductFixtureDir -Recurse -Force
    }
}

Write-Host "[OK] Vivid static memory admission smoke passed"
