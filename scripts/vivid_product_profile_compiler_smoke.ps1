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

$catalogPath = Join-Path $RepoRoot "Modules/ui/vivid/cmake/widget_catalog.cmake"
$soaGuiPath = Join-Path $RepoRoot "Modules/ui/vivid/core/soa_gui.cppm"
$catalogText = Get-Content -LiteralPath $catalogPath -Raw -Encoding UTF8
$catalogBlocks = [regex]::Matches(
    $catalogText,
    '(?ms)vivid_catalog_widget\(\s*(.*?)\r?\n\)')
$catalogSupported = @()
$catalogUnsupported = @()
foreach ($block in $catalogBlocks) {
    $body = $block.Groups[1].Value
    $kindMatch = [regex]::Match($body, '(?m)^[ \t]*KIND[ \t]+(\w+)')
    $supportMatch = [regex]::Match(
        $body,
        '(?m)^[ \t]*SCENE_SUPPORT[ \t]+(Supported|Unsupported)')
    if (-not $kindMatch.Success -or -not $supportMatch.Success) {
        throw "Widget catalog scene support declaration is incomplete"
    }
    if ($supportMatch.Groups[1].Value -eq "Supported") {
        $catalogSupported += $kindMatch.Groups[1].Value
    } else {
        $catalogUnsupported += $kindMatch.Groups[1].Value
    }
}

$soaGuiText = Get-Content -LiteralPath $soaGuiPath -Raw -Encoding UTF8
$recorderUnsupported = @(
    [regex]::Matches(
        $soaGuiText,
        '(?ms)case[ \t]+WidgetKind::(\w+):[ \t]*\r?\n[ \t]*unsupported_kind\(kind\)') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne "None" }
)
$sceneSupportDrift = @(Compare-Object -ReferenceObject ($catalogUnsupported | Sort-Object) -DifferenceObject ($recorderUnsupported | Sort-Object))
if ($sceneSupportDrift.Count -ne 0) {
    $sceneSupportDrift | Format-Table -AutoSize | Out-Host
    throw "Widget catalog SCENE_SUPPORT differs from SoaGui recorder support"
}
Write-Host (
    "[vivid-profile-compiler] scene_support supported={0} unsupported={1} recorder_match=1" -f
        $catalogSupported.Count,
        $catalogUnsupported.Count)

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
    TARGET Charm-ui-host
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
vivid_configure_product_target(
    TARGET Charm-ui-host
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
_vivid_target_get(Charm-ui-host PROFILE_FINGERPRINT _host_profile_fingerprint)
_vivid_target_get(Charm-ui-host TARGET_FINGERPRINT _host_target_fingerprint)

vivid_configure_product_target(
    TARGET h747_lab_player_md3
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
_vivid_target_get(h747_lab_player_md3 PROFILE_FINGERPRINT _h747_profile_fingerprint)
_vivid_target_get(h747_lab_player_md3 TARGET_FINGERPRINT _h747_target_fingerprint)

vivid_define_product_profile(
    NAME player_md3_equivalent
    EXTENDS player_md3
    PAYLOAD_CAPACITIES Label=096
    SOA_MAX_NODES 0384
    SOA_TEXT_ARENA_BYTES 024576
    STYLE_CLASS_MAX 016
    STYLE_RULE_CAP 08
    STYLE_METRICS_POOL_CAP 016
    DRAW_CMD_MAX_COMMANDS 01024
    DRAW_CMD_TEXT_BYTES 04096
    DRAW_CMD_BLOB_BYTES 02048
    FLOAT_WIDGETS TRUE)
_vivid_profile_get(player_md3_equivalent FINGERPRINT _equivalent_profile_fingerprint)
if(NOT _equivalent_profile_fingerprint STREQUAL _host_profile_fingerprint)
    message(FATAL_ERROR "Equivalent resolved profiles must have the same fingerprint")
endif()

vivid_configure_product_target(
    TARGET Charm-ui-host-equivalent
    PROFILE player_md3_equivalent
    SCREEN_WIDTH 0568
    SCREEN_HEIGHT 01210
    PIXEL_FORMAT RGB888
    LAYER_CACHE_SLOTS 02
    LAYER_CACHE_WIDTH 0568
    LAYER_CACHE_HEIGHT 01210
    RUNTIME_SCENE_INSTANCES 01
    STATIC_MEMORY_BUDGET_BYTES 06291456
    STATIC_MEMORY_MIN_HEADROOM_BYTES 0524288
    MAX_HOT_STACK_FRAME_BYTES 04096)
_vivid_target_get(
    Charm-ui-host-equivalent TARGET_FINGERPRINT _equivalent_target_fingerprint)
if(NOT _equivalent_target_fingerprint STREQUAL _host_target_fingerprint)
    message(FATAL_ERROR "Equivalent target envelopes must have the same fingerprint")
endif()

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
_vivid_profile_get(player_md3_debug WIDGET_KINDS _debug_kinds)
_vivid_profile_get(player_md3_debug PAYLOAD_CAPACITIES _debug_capacities)
vivid_widget_profile_resolve(
    _widget_modules _active_pools _defines
    PROFILE player_md3
    KINDS ${_kinds}
    PAYLOAD_CAPACITIES ${_capacities})
vivid_widget_profile_resolve(
    _debug_widget_modules _debug_active_pools _debug_defines
    PROFILE player_md3_debug
    KINDS ${_debug_kinds}
    PAYLOAD_CAPACITIES ${_debug_capacities})
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY positive-player-md3
    ROOT_MODULES ${_roots}
    INTERNAL_ROOT_MODULES ${_widget_modules})

list(LENGTH _kinds _kind_count)
list(LENGTH _active_pools _pool_count)
list(LENGTH _debug_kinds _debug_kind_count)
list(LENGTH _debug_active_pools _debug_pool_count)
if(NOT _kind_count EQUAL 15 OR NOT _pool_count EQUAL 11)
    message(FATAL_ERROR "Unexpected player_md3 catalog shape: kinds=${_kind_count} pools=${_pool_count}")
endif()
if(NOT _debug_kind_count EQUAL 17 OR NOT _debug_pool_count EQUAL 13)
    message(FATAL_ERROR
        "Unexpected player_md3_debug catalog shape: kinds=${_debug_kind_count} pools=${_debug_pool_count}")
endif()
foreach(_expected IN ITEMS
        charm.gfx.color
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
    "equivalent_profile_fingerprint=${_equivalent_profile_fingerprint}\n"
    "equivalent_target_fingerprint=${_equivalent_target_fingerprint}\n"
    "widget_kinds=${_kind_count}\n"
    "payload_pools=${_pool_count}\n"
    "debug_widget_kinds=${_debug_kind_count}\n"
    "debug_payload_pools=${_debug_pool_count}\n"
    "modules=${_module_count}\n"
    "sources=${_source_count}\n"
    "external_requirements=${_external_count}\n")
'@

$WidgetHelper = @'
function(add_test_widget kind id)
    vivid_catalog_widget(
        ID ${id}
        KIND ${kind}
        SCENE_SUPPORT Supported
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

$WidgetWithoutSceneSupport = $WidgetHelper -replace '(?m)^[ \t]*SCENE_SUPPORT Supported\r?\n', ''
$WidgetInvalidSceneSupport = $WidgetHelper.Replace(
    'SCENE_SUPPORT Supported',
    'SCENE_SUPPORT Maybe')

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

    Invoke-CMakeCase -Name "missing-scene-support" -Body ($WidgetWithoutSceneSupport + "`n" + @'
add_test_widget(Alpha 1)
'@) -ExpectSuccess $false -ExpectedPattern "vivid_catalog_widget requires SCENE_SUPPORT"

    Invoke-CMakeCase -Name "invalid-scene-support" -Body ($WidgetInvalidSceneSupport + "`n" + @'
add_test_widget(Alpha 1)
'@) -ExpectSuccess $false -ExpectedPattern "SCENE_SUPPORT must be Supported or Unsupported"

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

    Invoke-CMakeCase -Name "default-internal-root" -Body @'
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY default-internal-root
    ROOT_MODULES charm.core.object)
'@ -ExpectSuccess $false -ExpectedPattern "charm\.core\.object.*INTERNAL"

    Invoke-CMakeCase -Name "host-only-root" -Body @'
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY host-only-root
    ROOT_MODULES charm.gfx.snapshot)
'@ -ExpectSuccess $false -ExpectedPattern "charm\.gfx\.snapshot.*HOST_ONLY"

    Invoke-CMakeCase -Name "host-only-closure" -Body @'
vivid_build_module_inventory()
_vivid_register_module(charm.test.product_root "/product_root.cppm" "charm.gfx.snapshot")
vivid_module_policy(NAME charm.test.product_root ACCESS PRODUCT_ROOT)
vivid_compute_product_module_closure(
    _sources _modules _external
    KEY host-only-closure
    ROOT_MODULES charm.test.product_root)
'@ -ExpectSuccess $false -ExpectedPattern "closure reaches host-only module 'charm\.gfx\.snapshot'"

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

    Invoke-CMakeCase -Name "unknown-policy-module" -Body @'
vivid_build_module_inventory()
vivid_module_policy(NAME charm.test.does_not_exist ACCESS INTERNAL)
'@ -ExpectSuccess $false -ExpectedPattern "policy references unknown module 'charm\.test\.does_not_exist'"

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

    Invoke-CMakeCase -Name "payload-unknown" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE payload-unknown
    KINDS Container
    PAYLOAD_CAPACITIES DoesNotExist=1)
'@ -ExpectSuccess $false -ExpectedPattern "unknown payload pool 'DoesNotExist'"

    Invoke-CMakeCase -Name "payload-overflow" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE payload-overflow
    KINDS Label
    PAYLOAD_CAPACITIES Label=65536)
'@ -ExpectSuccess $false -ExpectedPattern "payload pool 'Label' must be in \[1, 65535\]"

    Invoke-CMakeCase -Name "unknown-widget-kind" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE unknown-widget-kind
    KINDS DoesNotExist)
'@ -ExpectSuccess $false -ExpectedPattern "unknown WidgetKind[\s\S]*'DoesNotExist'"

    Invoke-CMakeCase -Name "unsupported-widget-kind" -Body @'
vivid_widget_profile_resolve(
    _modules _pools _defines
    PROFILE unsupported-widget-kind
    KINDS Dropdown)
'@ -ExpectSuccess $false -ExpectedPattern "Dropdown[\s\S]*without Scene runtime support"

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

    Invoke-CMakeCase -Name "second-target-envelope" -Body ($MinimalProfiles + "`n" + @'
vivid_configure_product_target(
    TARGET Charm-ui PROFILE first
    SCREEN_WIDTH 32 SCREEN_HEIGHT 32 PIXEL_FORMAT RGB565
    LAYER_CACHE_SLOTS 1 LAYER_CACHE_WIDTH 32 LAYER_CACHE_HEIGHT 32
    RUNTIME_SCENE_INSTANCES 1 STATIC_MEMORY_BUDGET_BYTES 65536
    STATIC_MEMORY_MIN_HEADROOM_BYTES 4096 MAX_HOT_STACK_FRAME_BYTES 1024)
vivid_configure_product_target(
    TARGET Charm-ui PROFILE first
    SCREEN_WIDTH 32 SCREEN_HEIGHT 33 PIXEL_FORMAT RGB565
    LAYER_CACHE_SLOTS 1 LAYER_CACHE_WIDTH 32 LAYER_CACHE_HEIGHT 32
    RUNTIME_SCENE_INSTANCES 1 STATIC_MEMORY_BUDGET_BYTES 65536
    STATIC_MEMORY_MIN_HEADROOM_BYTES 4096 MAX_HOT_STACK_FRAME_BYTES 1024)
'@) -ExpectSuccess $false -ExpectedPattern "already configured with a different envelope"

    Invoke-CMakeCase -Name "duplicate-profile" -Body ($MinimalProfiles + "`n" + @'
vivid_define_product_profile(NAME first)
'@) -ExpectSuccess $false -ExpectedPattern "product profile 'first' is already defined"

    Invoke-CMakeCase -Name "legacy-variable" -Body @'
include("${REPO_ROOT}/Modules/ui/vivid/vivid.cmake")
set(CHARM_VIVID_PRODUCT_CORE_MODULES charm.ui.scene)
set(CHARM_VIVID_PRODUCT_GFX_MODULES charm.gfx.canvas)
set(CHARM_VIVID_PRODUCT_WIDGETS button)
set(CHARM_VIVID_PAYLOAD_CAP_LABEL 1)
vivid_reject_legacy_product_configuration()
'@ -ExpectSuccess $false -ExpectedPattern "CHARM_VIVID_PRODUCT_CORE_MODULES[\s\S]*CHARM_VIVID_PRODUCT_GFX_MODULES[\s\S]*CHARM_VIVID_PRODUCT_WIDGETS[\s\S]*CHARM_VIVID_PAYLOAD_CAP_LABEL"

    Invoke-CMakeCase -Name "self-inheritance" -Body @'
vivid_define_product_profile(NAME recursive EXTENDS recursive)
'@ -ExpectSuccess $false -ExpectedPattern "inheritance cycle"

    Invoke-CMakeCase -Name "unknown-base-profile" -Body @'
vivid_define_product_profile(NAME child EXTENDS does-not-exist)
'@ -ExpectSuccess $false -ExpectedPattern "extends unknown profile 'does-not-exist'"

    Invoke-CMakeCase -Name "unknown-field" -Body @'
vivid_define_product_profile(NAME unknown-field UNKNOWN_FIELD value)
'@ -ExpectSuccess $false -ExpectedPattern "unknown arguments: UNKNOWN_FIELD"

    Invoke-CMakeCase -Name "unknown-target-field" -Body @'
vivid_configure_product_target(TARGET Charm-ui PROFILE missing UNKNOWN_FIELD value)
'@ -ExpectSuccess $false -ExpectedPattern "unknown arguments: UNKNOWN_FIELD"
} finally {
    if (Test-Path -LiteralPath $FixtureRoot) {
        Remove-Item -LiteralPath $FixtureRoot -Recurse -Force
    }
}

Write-Host "[OK] Vivid product profile compiler smoke passed"
exit 0
