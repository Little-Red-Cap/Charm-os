param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$BuildDir = "",
    [string]$SummaryPath = "",
    [switch]$Clean,
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [string]$Path,
        [string]$BasePath = (Get-Location).Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
}

function Resolve-ToolPath {
    param(
        [string]$Tool
    )

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Get-CMakeCacheValue {
    param(
        [string]$CachePath,
        [string]$Name
    )

    if (-not (Test-Path $CachePath)) {
        return ""
    }

    $pattern = '^{0}:[^=]*=(.*)$' -f [regex]::Escape($Name)
    $match = Select-String -LiteralPath $CachePath -Pattern $pattern | Select-Object -First 1
    if ($null -eq $match) {
        return ""
    }

    return [string]$match.Matches[0].Groups[1].Value
}

function Resolve-NinjaPath {
    param(
        [string]$BuildPath
    )

    $cachePath = Join-Path $BuildPath "CMakeCache.txt"
    $makeProgram = Get-CMakeCacheValue -CachePath $cachePath -Name "CMAKE_MAKE_PROGRAM"
    if (-not [string]::IsNullOrWhiteSpace($makeProgram) -and (Test-Path $makeProgram)) {
        return (Resolve-Path $makeProgram).Path
    }

    return Resolve-ToolPath -Tool "ninja"
}

function Assert-SafeCleanBuildDir {
    param(
        [string]$RepoRoot,
        [string]$BuildPath
    )

    $resolvedRepoRoot = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $resolvedBuildPath = [System.IO.Path]::GetFullPath($BuildPath).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $repoPrefix = $resolvedRepoRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedBuildPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and $resolvedBuildPath -ne $resolvedRepoRoot) {
        throw "refusing to clean build directory outside repo: $resolvedBuildPath"
    }

    $leafName = Split-Path -Leaf $resolvedBuildPath
    if (-not $leafName.StartsWith("cmake-build-")) {
        throw "refusing to clean non cmake-build-* directory: $resolvedBuildPath"
    }
}

function Invoke-CheckedProcess {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$FailureMessage,
        [switch]$CaptureOutput
    )

    if ($CaptureOutput) {
        $output = & $FilePath @ArgumentList 2>&1
        $exitCode = $LASTEXITCODE
        $text = (($output | Out-String).Trim())
        if ($exitCode -ne 0) {
            if ([string]::IsNullOrWhiteSpace($text)) {
                throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
            }
            throw ("{0} (exit code {1}): {2}" -f $FailureMessage, $exitCode, $text)
        }

        return $text
    }

    $output = & $FilePath @ArgumentList 2>&1
    $exitCode = $LASTEXITCODE
    foreach ($line in @($output)) {
        Write-Host $line
    }

    if ($exitCode -ne 0) {
        $text = (($output | Out-String).Trim())
        if ([string]::IsNullOrWhiteSpace($text)) {
            throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
        }

        throw ("{0} (exit code {1}): {2}" -f $FailureMessage, $exitCode, $text)
    }

    return
}

function Resolve-ExecutablePath {
    param(
        [string]$BuildPath,
        [string]$TargetName
    )

    $candidates = @(
        (Join-Path $BuildPath ($TargetName + ".exe")),
        (Join-Path $BuildPath $TargetName)
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $found = Get-ChildItem -Path $BuildPath -Recurse -File | Where-Object {
        $_.BaseName -eq $TargetName -or $_.Name -eq ($TargetName + ".exe")
    } | Select-Object -First 1

    if ($found) {
        return $found.FullName
    }

    throw "executable not found for target: $TargetName"
}

function New-StepResult {
    param(
        [string]$Status,
        [int64]$ElapsedMs,
        [string]$Detail = "",
        [object]$Data = $null
    )

    $result = [ordered]@{
        status     = $Status
        elapsed_ms = $ElapsedMs
    }

    if (-not [string]::IsNullOrWhiteSpace($Detail)) {
        $result.detail = $Detail
    }

    if ($null -ne $Data) {
        $result.data = $Data
    }

    return [pscustomobject]$result
}

function Invoke-SmokeStep {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Write-Host ("==> {0}" -f $Name)
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $detail = & $Action
        $stopwatch.Stop()
        if ($detail -is [System.Collections.IDictionary]) {
            $script:steps[$Name] = New-StepResult `
                -Status "ok" `
                -ElapsedMs $stopwatch.ElapsedMilliseconds `
                -Detail ([string]$detail.detail) `
                -Data $detail.data
        } else {
            $script:steps[$Name] = New-StepResult -Status "ok" -ElapsedMs $stopwatch.ElapsedMilliseconds -Detail ([string]$detail)
        }
        return $detail
    } catch {
        $stopwatch.Stop()
        $script:steps[$Name] = New-StepResult -Status "fail" -ElapsedMs $stopwatch.ElapsedMilliseconds -Detail $_.Exception.Message
        throw
    }
}

function Test-TargetBoundary {
    param(
        [string]$TargetListText
    )

    $requiredTargets = @("Charm-audio", "sdl3-wav-demo")
    $forbiddenTargets = @("Charm-runtime", "Charm-media")
    $requiredPresent = [System.Collections.Generic.List[string]]::new()
    $requiredMissing = [System.Collections.Generic.List[string]]::new()
    $forbiddenPresent = [System.Collections.Generic.List[string]]::new()
    $forbiddenAbsent = [System.Collections.Generic.List[string]]::new()

    foreach ($target in $requiredTargets) {
        if ($TargetListText.Contains($target)) {
            $requiredPresent.Add($target) | Out-Null
        } else {
            $requiredMissing.Add($target) | Out-Null
        }
    }

    foreach ($target in $forbiddenTargets) {
        if ($TargetListText.Contains($target)) {
            $forbiddenPresent.Add($target) | Out-Null
        } else {
            $forbiddenAbsent.Add($target) | Out-Null
        }
    }

    $data = [pscustomobject]@{
        required_present  = @($requiredPresent)
        required_missing  = @($requiredMissing)
        forbidden_present = @($forbiddenPresent)
        forbidden_absent  = @($forbiddenAbsent)
    }

    if ($requiredMissing.Count -gt 0 -or $forbiddenPresent.Count -gt 0) {
        $parts = [System.Collections.Generic.List[string]]::new()
        if ($requiredMissing.Count -gt 0) {
            $parts.Add(("missing required targets: {0}" -f ($requiredMissing -join ", "))) | Out-Null
        }
        if ($forbiddenPresent.Count -gt 0) {
            $parts.Add(("forbidden targets present: {0}" -f ($forbiddenPresent -join ", "))) | Out-Null
        }

        return [pscustomobject]@{
            ok     = $false
            detail = ($parts -join "; ")
            data   = $data
        }
    }

    return [pscustomobject]@{
        ok     = $true
        detail = "Charm-audio boundary target list is clean"
        data   = $data
    }
}

function Write-SmokeSummary {
    param(
        [string]$Path,
        [object]$Summary
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    Ensure-ParentDirectory -Path $Path
    $Summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding utf8
    Write-Host ("[SUMMARY] {0}" -f $Path)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$sourceDir = Join-Path $repoRoot "Examples/audio/sdl3_wav_demo"
$samplePath = Join-Path $sourceDir "sample.flac"
$targetName = "sdl3-wav-demo"
$resolvedBuildDir = if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    Join-Path $repoRoot "cmake-build-audio-sdl3-smoke"
} else {
    Resolve-FullPath -Path $BuildDir -BasePath $repoRoot
}
$resolvedSummaryPath = Resolve-FullPath -Path $SummaryPath -BasePath $repoRoot
$cmake = Resolve-ToolPath -Tool $CMakeExe
$steps = [ordered]@{}
$hasFailure = $false
$failure = ""

if (-not (Test-Path (Join-Path $sourceDir "CMakeLists.txt"))) {
    throw "missing audio demo CMakeLists.txt: $sourceDir"
}

if (-not (Test-Path $samplePath)) {
    throw "missing audio demo sample: $samplePath"
}

Push-Location $repoRoot
try {
    try {
        if ($Clean -and (Test-Path $resolvedBuildDir)) {
            Assert-SafeCleanBuildDir -RepoRoot $repoRoot -BuildPath $resolvedBuildDir
            Write-Host ("==> clean {0}" -f $resolvedBuildDir)
            Remove-Item -LiteralPath $resolvedBuildDir -Recurse -Force
        }

        Invoke-SmokeStep -Name "configure" -Action {
            Invoke-CheckedProcess `
                -FilePath $cmake `
                -ArgumentList @("-S", $sourceDir, "-B", $resolvedBuildDir, "-G", $Generator) `
                -FailureMessage "cmake configure failed"
            return "configured"
        } | Out-Null

        Invoke-SmokeStep -Name "build" -Action {
            $buildArgs = @("--build", $resolvedBuildDir)
            if ($Jobs -gt 0) {
                $buildArgs += @("--parallel", $Jobs)
            }

            Invoke-CheckedProcess `
                -FilePath $cmake `
                -ArgumentList $buildArgs `
                -FailureMessage "cmake build failed"
            return "built"
        } | Out-Null

        Invoke-SmokeStep -Name "target_boundary" -Action {
            $ninja = Resolve-NinjaPath -BuildPath $resolvedBuildDir
            $targetsText = Invoke-CheckedProcess `
                -FilePath $ninja `
                -ArgumentList @("-C", $resolvedBuildDir, "-t", "targets") `
                -FailureMessage "ninja target list failed" `
                -CaptureOutput
            $boundary = Test-TargetBoundary -TargetListText $targetsText
            if (-not $boundary.ok) {
                throw $boundary.detail
            }

            return @{
                detail = $boundary.detail
                data   = $boundary.data
            }
        } | Out-Null

        $exePath = Resolve-ExecutablePath -BuildPath $resolvedBuildDir -TargetName $targetName
        $oldAudioDriver = $env:SDL_AUDIODRIVER
        try {
            $env:SDL_AUDIODRIVER = "dummy"

            Invoke-SmokeStep -Name "tone" -Action {
                $output = Invoke-CheckedProcess `
                    -FilePath $exePath `
                    -ArgumentList @("--tone", "--seconds", "1") `
                    -FailureMessage "tone smoke run failed" `
                    -CaptureOutput
                if ([string]::IsNullOrWhiteSpace($output)) {
                    return "tone completed"
                }

                return ($output -replace "`r?`n", " | ")
            } | Out-Null

            Invoke-SmokeStep -Name "flac" -Action {
                $output = Invoke-CheckedProcess `
                    -FilePath $exePath `
                    -ArgumentList @($samplePath, "--seconds", "1") `
                    -FailureMessage "flac smoke run failed" `
                    -CaptureOutput
                if ([string]::IsNullOrWhiteSpace($output)) {
                    return "flac completed"
                }

                return ($output -replace "`r?`n", " | ")
            } | Out-Null
        } finally {
            $env:SDL_AUDIODRIVER = $oldAudioDriver
        }
    } catch {
        $hasFailure = $true
        $failure = $_.Exception.Message
        Write-Host ("FAIL {0}" -f $failure)
    }
} finally {
    Pop-Location
}

$summary = [pscustomobject]@{
    schema       = "audio.sdl3_wav_demo_smoke.summary/v1"
    generated_at = (Get-Date).ToString("o")
    repo_root    = $repoRoot
    source_dir   = $sourceDir
    build_dir    = $resolvedBuildDir
    cmake        = $cmake
    generator    = $Generator
    target       = $targetName
    sample       = $samplePath
    mode         = [pscustomobject]@{
        clean = [bool]$Clean
        jobs  = $Jobs
    }
    has_failure  = $hasFailure
    failure      = $failure
    steps        = [pscustomobject]$steps
}

Write-SmokeSummary -Path $resolvedSummaryPath -Summary $summary

if ($hasFailure) {
    exit 1
}

exit 0
