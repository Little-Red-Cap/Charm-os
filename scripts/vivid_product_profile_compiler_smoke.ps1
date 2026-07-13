param(
    [string]$BuildDir = "Examples/ui/vivid/soa_demo/cmake-build-soa-ci",
    [string]$CMakeExe = "cmake"
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

$FixtureRoot = Join-Path $BuildDir "vivid-product-profile-compiler-smoke"
$fixturePrefix = $BuildDir.TrimEnd('\') + '\'
if (-not $FixtureRoot.StartsWith($fixturePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Fixture path escaped the build directory: $FixtureRoot"
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

$Prelude = @'
cmake_minimum_required(VERSION 4.0)
if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif()
file(TO_CMAKE_PATH "${REPO_ROOT}" REPO_ROOT)
include("${REPO_ROOT}/Modules/ui/vivid/cmake/product_profile_compiler.cmake")
'@

function Invoke-CMakeCase {
    param(
        [string]$Name,
        [string]$Body,
        [bool]$ExpectSuccess,
        [string]$ExpectedPattern = ""
    )

    $casePath = Join-Path $FixtureRoot "$Name.cmake"
    Write-Utf8NoBom -Path $casePath -Content ($Prelude + "`n" + $Body + "`n")
    $cmakeArgs = @(
        "-DREPO_ROOT=$($RepoRoot.Replace('\', '/'))",
        "-DCASE_OUTPUT=$((Join-Path $FixtureRoot "$Name.txt").Replace('\', '/'))",
        "-P", $casePath
    )

    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $CMakeExe @cmakeArgs 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }

    $success = $exitCode -eq 0
    $outputText = $output -join "`n"
    if ($success -ne $ExpectSuccess) {
        $output | Out-Host
        throw "Unexpected result for profile compiler case '$Name': success=$success expected=$ExpectSuccess"
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedPattern) -and
        $outputText -notmatch $ExpectedPattern) {
        $output | Out-Host
        throw "Profile compiler case '$Name' did not report '$ExpectedPattern'"
    }

    $result = if ($ExpectSuccess) { "pass" } else { "rejected" }
    Write-Host "[vivid-profile-compiler] case=$Name result=$result"
}

$PositiveCase = @'
set(CHARM_ROOT "${REPO_ROOT}")
set(CHARM_PLAYER_DEBUG_UI OFF)
include("${REPO_ROOT}/Examples/project/player/cmake/player_md3_vivid_product.cmake")

vivid_configure_product_target(
    TARGET Charm-ui
    PROFILE player_md3
    SCREEN_WIDTH 568
    SCREEN_HEIGHT 1210
    PIXEL_FORMAT RGB888
    LAYER_CACHE_SLOTS 2
    LAYER_CACHE_WIDTH 568
    LAYER_CACHE_HEIGHT 1210
    RUNTIME_SCENE_INSTANCES 1
    STATIC_MEMORY_BUDGET_BYTES 6291456
    STATIC_MEMORY_MIN_HEADROOM_BYTES 524288
    MAX_HOT_STACK_FRAME_BYTES 4096)
_vivid_target_get(Charm-ui PROFILE_FINGERPRINT _host_profile_fingerprint)
_vivid_target_get(Charm-ui TARGET_FINGERPRINT _host_target_fingerprint)

vivid_configure_product_target(
    TARGET Charm-ui
    PROFILE player_md3
    SCREEN_WIDTH 720
    SCREEN_HEIGHT 1280
    PIXEL_FORMAT RGB888
    LAYER_CACHE_SLOTS 1
    LAYER_CACHE_WIDTH 720
    LAYER_CACHE_HEIGHT 1280
    RUNTIME_SCENE_INSTANCES 1
    STATIC_MEMORY_BUDGET_BYTES 5242880
    STATIC_MEMORY_MIN_HEADROOM_BYTES 524288
    MAX_HOT_STACK_FRAME_BYTES 4096)
_vivid_target_get(Charm-ui PROFILE_FINGERPRINT _h747_profile_fingerprint)
_vivid_target_get(Charm-ui TARGET_FINGERPRINT _h747_target_fingerprint)

if(NOT _host_profile_fingerprint STREQUAL _h747_profile_fingerprint)
    message(FATAL_ERROR "Host and H747 profile fingerprints differ")
endif()
if(_host_target_fingerprint STREQUAL _h747_target_fingerprint)
    message(FATAL_ERROR "Host and H747 target fingerprints must differ")
endif()
string(LENGTH "${_host_profile_fingerprint}" _profile_fingerprint_length)
if(NOT _profile_fingerprint_length EQUAL 64 OR
   NOT _host_profile_fingerprint MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Invalid profile fingerprint '${_host_profile_fingerprint}'")
endif()

_vivid_profile_get(player_md3 ROOT_MODULES _roots)
_vivid_profile_get(player_md3 WIDGET_KINDS _kinds)
_vivid_profile_get(player_md3 PAYLOAD_CAPACITIES _capacities)
vivid_widget_profile_resolve(
    _widget_modules _active_pools _defines
    PROFILE player_md3
    KINDS ${_kinds}
    PAYLOAD_CAPACITIES ${_capacities})
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY positive-player-md3
    ROOT_MODULES ${_roots}
    INTERNAL_ROOT_MODULES ${_widget_modules})

list(LENGTH _kinds _kind_count)
list(LENGTH _active_pools _pool_count)
if(NOT _kind_count EQUAL 30 OR NOT _pool_count EQUAL 12)
    message(FATAL_ERROR "Unexpected player_md3 catalog shape: kinds=${_kind_count} pools=${_pool_count}")
endif()
foreach(_expected IN ITEMS
        charm.gfx.color
        charm.widgets.battery_gasgauge
        charm.core.soa_kernel:actions
        charm.core.soa_kernel:behavior
        charm.core.soa_kernel:input_core
        charm.core.soa_kernel:layout_state
        charm.core.soa_kernel:payload
        charm.core.soa_kernel:payload_lists
        charm.core.soa_kernel:payload_views
        charm.core.soa_kernel:semantic
        charm.core.soa_kernel:storage)
    if(NOT _expected IN_LIST _modules)
        message(FATAL_ERROR "Product closure is missing '${_expected}'")
    endif()
endforeach()
foreach(_forbidden IN ITEMS
        charm.gfx.snapshot
        charm.gfx.host_tools
        charm.ui.scene.motion_runtime
        charm.ui.scene.page_transition)
    if(_forbidden IN_LIST _modules)
        message(FATAL_ERROR "Product closure contains forbidden module '${_forbidden}'")
    endif()
endforeach()

file(TO_CMAKE_PATH "${CASE_OUTPUT}" CASE_OUTPUT)
list(LENGTH _modules _module_count)
list(LENGTH _sources _source_count)
list(LENGTH _external _external_count)
file(WRITE "${CASE_OUTPUT}"
    "profile_fingerprint=${_host_profile_fingerprint}\n"
    "host_target_fingerprint=${_host_target_fingerprint}\n"
    "h747_target_fingerprint=${_h747_target_fingerprint}\n"
    "widget_kinds=${_kind_count}\n"
    "payload_pools=${_pool_count}\n"
    "modules=${_module_count}\n"
    "sources=${_source_count}\n"
    "external_requirements=${_external_count}\n")
'@

$WidgetHelper = @'
function(add_test_widget kind id)
    vivid_catalog_widget(
        ID ${id}
        KIND ${kind}
        RUNTIME_ONLY
        CPP_TYPE TestWidget
        THEME_BASE None
        FACTORY none
        FACTORY_POOL none
        FACTORY_CREATE None
        PAYLOAD_POOL None
        STYLE Readonly
        CLICK None
        CLICK_INDEX None
        GROUP_KIND None
        WHEEL_TARGET None
        DRAG_BEHAVIOR None
        DRAG_BEHAVIOR_ONLY None
        WHEEL_TARGET_ONLY None
        SCROLL_AXIS None
        WHEEL_AXIS None)
endfunction()
'@

$MinimalProfiles = @'
vivid_define_product_profile(
    NAME first
    ROOT_MODULES charm.ui.vivid
    WIDGET_KINDS Container
    SOA_MAX_NODES 16
    SOA_TEXT_ARENA_BYTES 128
    STYLE_CLASS_MAX 2
    STYLE_RULE_CAP 2
    STYLE_METRICS_POOL_CAP 2
    DRAW_CMD_MAX_COMMANDS 16
    DRAW_CMD_TEXT_BYTES 128
    DRAW_CMD_BLOB_BYTES 64
    FLOAT_WIDGETS OFF)
vivid_define_product_profile(
    NAME second
    EXTENDS first
    ROOT_MODULES charm.gfx.color)
'@

try {
    if (Test-Path -LiteralPath $FixtureRoot) {
        Remove-Item -LiteralPath $FixtureRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $FixtureRoot -Force | Out-Null

    Invoke-CMakeCase -Name "positive" -Body $PositiveCase -ExpectSuccess $true
    $positiveEvidence = Join-Path $FixtureRoot "positive.txt"
    if (-not (Test-Path -LiteralPath $positiveEvidence)) {
        throw "Positive profile compiler evidence was not generated"
    }
    Get-Content -LiteralPath $positiveEvidence -Encoding UTF8 | ForEach-Object {
        Write-Host "[vivid-profile-compiler] $_"
    }

    Invoke-CMakeCase -Name "duplicate-kind" -Body ($WidgetHelper + "`n" + @'
add_test_widget(Alpha 1)
add_test_widget(Alpha 2)
'@) -ExpectSuccess $false -ExpectedPattern "Duplicate Vivid WidgetKind 'Alpha'"

    Invoke-CMakeCase -Name "duplicate-id" -Body ($WidgetHelper + "`n" + @'
add_test_widget(Alpha 1)
add_test_widget(Beta 1)
'@) -ExpectSuccess $false -ExpectedPattern "Duplicate Vivid WidgetKind ID '1'"

    Invoke-CMakeCase -Name "duplicate-module" -Body @'
_vivid_register_module(charm.test.duplicate "/first.cppm" "")
_vivid_register_module(charm.test.duplicate "/second.cppm" "")
'@ -ExpectSuccess $false -ExpectedPattern "Duplicate Vivid module 'charm.test.duplicate'"

    Invoke-CMakeCase -Name "unknown-root" -Body @'
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY unknown-root
    ROOT_MODULES charm.ui.does_not_exist)
'@ -ExpectSuccess $false -ExpectedPattern "Unknown Vivid PRODUCT root module"

    Invoke-CMakeCase -Name "internal-root" -Body @'
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY internal-root
    ROOT_MODULES charm.core.soa_kernel)
'@ -ExpectSuccess $false -ExpectedPattern "charm\.core\.soa_kernel.*INTERNAL"

    Invoke-CMakeCase -Name "host-only-root" -Body @'
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY host-only-root
    ROOT_MODULES charm.gfx.snapshot)
'@ -ExpectSuccess $false -ExpectedPattern "charm\.gfx\.snapshot.*HOST_ONLY"

    Invoke-CMakeCase -Name "module-cycle" -Body @'
vivid_build_module_inventory()
_vivid_register_module(charm.test.cycle_a "/cycle_a.cppm" "charm.test.cycle_b")
_vivid_register_module(charm.test.cycle_b "/cycle_b.cppm" "charm.test.cycle_a")
vivid_module_policy(NAME charm.test.cycle_a ACCESS PRODUCT_ROOT)
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY module-cycle
    ROOT_MODULES charm.test.cycle_a)
'@ -ExpectSuccess $false -ExpectedPattern "Vivid module dependency cycle"

    Invoke-CMakeCase -Name "payload-missing" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE payload-missing
    KINDS Label)
'@ -ExpectSuccess $false -ExpectedPattern "must declare payload pool 'Label'"

    Invoke-CMakeCase -Name "payload-without-consumer" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE payload-without-consumer
    KINDS Container
    PAYLOAD_CAPACITIES Label=1)
'@ -ExpectSuccess $false -ExpectedPattern "payload pool 'Label'[\s\S]*active consumer"

    Invoke-CMakeCase -Name "second-target-profile" -Body ($MinimalProfiles + "`n" + @'
vivid_configure_product_target(
    TARGET Charm-ui PROFILE first
    SCREEN_WIDTH 32 SCREEN_HEIGHT 32 PIXEL_FORMAT RGB565
    LAYER_CACHE_SLOTS 1 LAYER_CACHE_WIDTH 32 LAYER_CACHE_HEIGHT 32
    RUNTIME_SCENE_INSTANCES 1 STATIC_MEMORY_BUDGET_BYTES 65536
    STATIC_MEMORY_MIN_HEADROOM_BYTES 4096 MAX_HOT_STACK_FRAME_BYTES 1024)
vivid_configure_product_target(
    TARGET Charm-ui PROFILE second
    SCREEN_WIDTH 32 SCREEN_HEIGHT 32 PIXEL_FORMAT RGB565
    LAYER_CACHE_SLOTS 1 LAYER_CACHE_WIDTH 32 LAYER_CACHE_HEIGHT 32
    RUNTIME_SCENE_INSTANCES 1 STATIC_MEMORY_BUDGET_BYTES 65536
    STATIC_MEMORY_MIN_HEADROOM_BYTES 4096 MAX_HOT_STACK_FRAME_BYTES 1024)
'@) -ExpectSuccess $false -ExpectedPattern "already uses profile 'first'"

    Invoke-CMakeCase -Name "duplicate-profile" -Body ($MinimalProfiles + "`n" + @'
vivid_define_product_profile(NAME first)
'@) -ExpectSuccess $false -ExpectedPattern "product profile 'first' is already defined"

    Invoke-CMakeCase -Name "legacy-variable" -Body @'
include("${REPO_ROOT}/Modules/ui/vivid/vivid.cmake")
set(CHARM_VIVID_PRODUCT_WIDGETS button)
set(CHARM_VIVID_PAYLOAD_CAP_LABEL 1)
vivid_reject_legacy_product_configuration()
'@ -ExpectSuccess $false -ExpectedPattern "CHARM_VIVID_PRODUCT_WIDGETS[\s\S]*CHARM_VIVID_PAYLOAD_CAP_LABEL"

    Invoke-CMakeCase -Name "self-inheritance" -Body @'
vivid_define_product_profile(NAME recursive EXTENDS recursive)
'@ -ExpectSuccess $false -ExpectedPattern "inheritance cycle"

    Invoke-CMakeCase -Name "unknown-field" -Body @'
vivid_define_product_profile(NAME unknown-field UNKNOWN_FIELD value)
'@ -ExpectSuccess $false -ExpectedPattern "unknown arguments: UNKNOWN_FIELD"
} finally {
    if (Test-Path -LiteralPath $FixtureRoot) {
        Remove-Item -LiteralPath $FixtureRoot -Recurse -Force
    }
}

Write-Host "[OK] Vivid product profile compiler smoke passed"
