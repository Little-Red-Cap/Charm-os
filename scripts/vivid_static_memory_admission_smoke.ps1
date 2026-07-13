param(
    [string]$BuildDir = "Examples/ui/vivid/soa_demo/cmake-build-soa-ci"
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}
$SourceDir = Join-Path $RepoRoot "Examples/ui/vivid/soa_demo"
$Manifest = Join-Path $BuildDir "Charm/generated/vivid/static_memory_admission.txt"
$StackSourceManifest = Join-Path $BuildDir "Charm/generated/vivid/stack_usage_sources.txt"

function Invoke-VividConfigure {
    param(
        [string]$FeatureSet,
        [string]$Budget,
        [string]$Headroom,
        [string[]]$ExtraArgs = @(),
        [bool]$ExpectSuccess = $true
    )

    $args = @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCHARM_VIVID_FEATURESET=$FeatureSet",
        "-DCHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES=$Budget",
        "-DCHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES=$Headroom"
    ) + $ExtraArgs
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & cmake @args 2>&1
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

function Read-AdmissionManifest {
    if (-not (Test-Path -LiteralPath $Manifest)) {
        throw "Missing Vivid static memory manifest: $Manifest"
    }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Manifest -Encoding UTF8) {
        if ($line -match '^([^=]+)=(.*)$') {
            $values[$Matches[1]] = $Matches[2]
        }
    }
    return $values
}

function Assert-Admission {
    param(
        [hashtable]$Values,
        [string]$FeatureSet,
        [string]$Status,
        [int64]$MinimumHeadroom
    )
    if ($Values.featureset -ne $FeatureSet -or $Values.status -ne $Status) {
        throw "Unexpected manifest identity: featureset=$($Values.featureset) status=$($Values.status)"
    }
    $upper = [int64]$Values.upper_bound_bytes
    $budget = [int64]$Values.budget_bytes
    $headroom = [int64]$Values.configured_headroom_bytes
    if ($upper -le 0) { throw "Upper bound must be positive" }
    foreach ($field in @(
        "command_buffer_upper_bytes",
        "draw_cmd_compaction_workspace_upper_bytes",
        "draw_cmd_executor_workspace_upper_bytes",
        "soa_traversal_workspace_upper_bytes"
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
    if ($Status -eq "admitted" -and ($budget -le 0 -or $headroom -lt $MinimumHeadroom)) {
        throw "Admission headroom is insufficient: upper=$upper budget=$budget headroom=$headroom"
    }
    Write-Host "[vivid-static-memory] featureset=$FeatureSet status=$Status upper=$upper budget=$budget headroom=$headroom"
}

function Assert-ProductStackSources {
    if (-not (Test-Path -LiteralPath $StackSourceManifest)) {
        throw "Missing Vivid stack source manifest: $StackSourceManifest"
    }
    $sources = @(Get-Content -LiteralPath $StackSourceManifest -Encoding UTF8)
    foreach ($expected in @(
        "Modules/ui/vivid/charm.ui.vivid.cppm",
        "Modules/ui/vivid/core/style_sheet.cppm",
        "Modules/ui/vivid/gfx/svg.cppm",
        "Modules/ui/vivid/widgets/button.cppm"
    )) {
        if ($sources -notcontains $expected) {
            throw "PRODUCT stack source manifest is missing selected Vivid module: $expected"
        }
    }
    if ($sources -contains "Modules/ui/vivid/gfx/snapshot.cppm") {
        throw "PRODUCT stack source manifest contains host-only snapshot module"
    }
    Write-Host "[vivid-static-memory] PRODUCT stack_sources=$($sources.Count) scope=all-selected"
}

$fullArgs = @(
    "-DCHARM_VIVID_RUNTIME_SCENE_INSTANCES=1",
    "-DCHARM_VIVID_SOA_MAX_NODES=256",
    "-DCHARM_VIVID_SOA_TEXT_ARENA_BYTES=",
    "-DCHARM_VIVID_STYLE_CLASS_MAX=256",
    "-DCHARM_VIVID_STYLE_RULE_CAP=32",
    "-DCHARM_VIVID_STYLE_METRICS_POOL_CAP=64",
    "-DCHARM_VIVID_SCREEN_WIDTH=800",
    "-DCHARM_VIVID_SCREEN_HEIGHT=480",
    "-DCHARM_VIVID_SCREEN_PIXEL_FORMAT=RGB888",
    "-DCHARM_VIVID_LAYER_CACHE_SLOTS=1",
    "-DCHARM_VIVID_LAYER_CACHE_WIDTH=400",
    "-DCHARM_VIVID_LAYER_CACHE_HEIGHT=240",
    "-DCHARM_VIVID_PRODUCT_WIDGETS="
)

try {
    Invoke-VividConfigure -FeatureSet "FULL" -Budget "" -Headroom "" -ExtraArgs $fullArgs | Out-Null
    Assert-Admission -Values (Read-AdmissionManifest) -FeatureSet "FULL" -Status "profile_only" -MinimumHeadroom 0

    $missingSceneCount = Invoke-VividConfigure -FeatureSet "MCU_MIN" -Budget "1835008" -Headroom "262144" -ExtraArgs ($fullArgs + @("-UCHARM_VIVID_RUNTIME_SCENE_INSTANCES")) -ExpectSuccess $false
    if ($missingSceneCount -notmatch 'CHARM_VIVID_RUNTIME_SCENE_INSTANCES') {
        throw "MCU_MIN missing-scene-count failure did not report the expected rule"
    }

    $missingBudget = Invoke-VividConfigure -FeatureSet "MCU_MIN" -Budget "" -Headroom "" -ExtraArgs $fullArgs -ExpectSuccess $false
    if ($missingBudget -notmatch 'CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES') {
        throw "MCU_MIN missing-budget failure did not report the expected rule"
    }

    $zeroHeadroom = Invoke-VividConfigure -FeatureSet "MCU_MIN" -Budget "1835008" -Headroom "0" -ExtraArgs $fullArgs -ExpectSuccess $false
    if ($zeroHeadroom -notmatch 'CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES must be > 0') {
        throw "MCU_MIN zero-headroom failure did not report the expected rule"
    }

    Invoke-VividConfigure -FeatureSet "MCU_MIN" -Budget "1835008" -Headroom "262144" -ExtraArgs $fullArgs | Out-Null
    Assert-Admission -Values (Read-AdmissionManifest) -FeatureSet "MCU_MIN" -Status "admitted" -MinimumHeadroom 262144

    $productArgs = @(
        "-DCHARM_VIVID_PRODUCT_WIDGETS=button;label;image;image_box;list_view;scrollbar;scroll_container;scroll_dirty;progress;battery_gasgauge;progress_bar_simple;progress_bar_drill;segmented_control;slider;switcher;dropdown;perf_overlay;busy_wheel;chart;cloudy_glass;console_box;crt_screen;dynamic_nebula;foldable_panel;histogram;histogram_view;meter_pointer;spectrum_view;spinning_wheel",
        "-DCHARM_VIVID_SOA_MAX_NODES=384",
        "-DCHARM_VIVID_SOA_TEXT_ARENA_BYTES=24576",
        "-DCHARM_VIVID_STYLE_CLASS_MAX=16",
        "-DCHARM_VIVID_STYLE_RULE_CAP=8",
        "-DCHARM_VIVID_STYLE_METRICS_POOL_CAP=16",
        "-DCHARM_VIVID_PAYLOAD_CAP_LABEL=96",
        "-DCHARM_VIVID_PAYLOAD_CAP_BUTTON=56",
        "-DCHARM_VIVID_PAYLOAD_CAP_IMAGE=24",
        "-DCHARM_VIVID_PAYLOAD_CAP_LIST_VIEW=4",
        "-DCHARM_VIVID_PAYLOAD_CAP_SEGMENTED_CONTROL=4",
        "-DCHARM_VIVID_PAYLOAD_CAP_SLIDER=12",
        "-DCHARM_VIVID_PAYLOAD_CAP_SWITCH=4",
        "-DCHARM_VIVID_PAYLOAD_CAP_PROGRESS=10",
        "-DCHARM_VIVID_PAYLOAD_CAP_SCROLLBAR=5",
        "-DCHARM_VIVID_PAYLOAD_CAP_SCROLL_CONTAINER=5",
        "-DCHARM_VIVID_PAYLOAD_CAP_TEXT_LIST=4",
        "-DCHARM_VIVID_PAYLOAD_CAP_SPINNER=4",
        "-DCHARM_VIVID_SCREEN_WIDTH=720",
        "-DCHARM_VIVID_SCREEN_HEIGHT=1280",
        "-DCHARM_VIVID_SCREEN_PIXEL_FORMAT=RGB888",
        "-DCHARM_VIVID_LAYER_CACHE_SLOTS=1",
        "-DCHARM_VIVID_LAYER_CACHE_WIDTH=720",
        "-DCHARM_VIVID_LAYER_CACHE_HEIGHT=1280"
    )
    Invoke-VividConfigure -FeatureSet "PRODUCT" -Budget "5242880" -Headroom "524288" -ExtraArgs $productArgs | Out-Null
    Assert-Admission -Values (Read-AdmissionManifest) -FeatureSet "PRODUCT" -Status "admitted" -MinimumHeadroom 524288
    Assert-ProductStackSources

    $tooSmall = Invoke-VividConfigure -FeatureSet "PRODUCT" -Budget "3145728" -Headroom "524288" -ExtraArgs $productArgs -ExpectSuccess $false
    if ($tooSmall -notmatch 'Vivid static memory admission failed') {
        throw "PRODUCT insufficient-budget failure did not report the expected rule"
    }
} finally {
    Invoke-VividConfigure -FeatureSet "FULL" -Budget "" -Headroom "" -ExtraArgs $fullArgs | Out-Null
}

Write-Host "[OK] Vivid static memory admission smoke passed"
