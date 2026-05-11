param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$BuildDir = "cmake-build-vivid",
    [string]$OutputRoot = "out/net-lab-route-precedence-smoke",
    [int]$Jobs = 0,
    [switch]$Fresh
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

function Resolve-ToolPath {
    param(
        [string]$Tool
    )

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
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

function Assert-PathInsideRoot {
    param(
        [string]$Path,
        [string]$Root
    )

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $Root
    $rootPrefix = $resolvedRoot.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar

    if (-not $resolvedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "refuse to touch path outside repo: $resolvedPath"
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path,
        [string]$Root
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    Assert-PathInsideRoot -Path $Path -Root $Root
    Remove-Item -LiteralPath $Path -Recurse -Force
}

function Get-CMakeHomeDirectory {
    param(
        [string]$BuildPath
    )

    $cachePath = Join-Path $BuildPath "CMakeCache.txt"
    if (-not (Test-Path $cachePath)) {
        return ""
    }

    $match = Select-String `
        -LiteralPath $cachePath `
        -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$" `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $match) {
        return ""
    }

    return $match.Matches[0].Groups[1].Value
}

function Invoke-LoggedExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage,
        [string]$Phase
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f $Phase)

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Invoke-LoggedProgram {
    param(
        [string]$Executable,
        [string]$LogPath,
        [string]$FailureMessage
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host "==> run"

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $Executable 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $output | Tee-Object -FilePath $LogPath | Out-Host

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return @($output | ForEach-Object { [string]$_ })
}

function Resolve-ExecutablePath {
    param(
        [string]$BuildDir,
        [string]$TargetName
    )

    $names = @(
        "$TargetName.exe",
        $TargetName
    )
    $subdirs = @("", "Debug", "Release", "RelWithDebInfo", "MinSizeRel")

    foreach ($subdir in $subdirs) {
        foreach ($name in $names) {
            $candidate = if ([string]::IsNullOrWhiteSpace($subdir)) {
                Join-Path $BuildDir $name
            } else {
                Join-Path (Join-Path $BuildDir $subdir) $name
            }

            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    foreach ($name in $names) {
        $match = Get-ChildItem -LiteralPath $BuildDir -Recurse -File -Filter $name -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $match) {
            return $match.FullName
        }
    }

    throw "executable not found for target: $TargetName"
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$cmake = Resolve-ToolPath -Tool $CMakeExe
$targetName = "net-lab-route-precedence-smoke"
$exampleRoot = Resolve-FullPath -Path (Join-Path $repoRoot "Examples\io\net\net_lab_route_precedence_smoke")
$buildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    Resolve-FullPath -Path $BuildDir
} else {
    Resolve-FullPath -Path (Join-Path $repoRoot $BuildDir)
}
$outputRootPath = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    Resolve-FullPath -Path $OutputRoot
} else {
    Resolve-FullPath -Path (Join-Path $repoRoot $OutputRoot)
}
$configureLogPath = Join-Path $outputRootPath "configure.log"
$buildLogPath = Join-Path $outputRootPath "build.log"
$runLogPath = Join-Path $outputRootPath "run.log"
$checkTextPath = Join-Path $outputRootPath "check.txt"

if (-not (Test-Path $exampleRoot)) {
    throw "example root not found: $exampleRoot"
}

if ($Fresh) {
    Remove-PathIfExists -Path $buildDir -Root $repoRoot
    Remove-PathIfExists -Path $outputRootPath -Root $repoRoot
} else {
    $cmakeHomeDirectory = Get-CMakeHomeDirectory -BuildPath $buildDir
    if (-not [string]::IsNullOrWhiteSpace($cmakeHomeDirectory)) {
        $resolvedCMakeHomeDirectory = Resolve-FullPath -Path $cmakeHomeDirectory
        if (-not $resolvedCMakeHomeDirectory.Equals($exampleRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Host ("==> reset build dir configured for: {0}" -f $resolvedCMakeHomeDirectory)
            Remove-PathIfExists -Path $buildDir -Root $repoRoot
        }
    }
}
Ensure-Directory -Path $outputRootPath

$configureArgs = [System.Collections.Generic.List[string]]::new()
$configureArgs.Add("-S") | Out-Null
$configureArgs.Add($exampleRoot) | Out-Null
$configureArgs.Add("-B") | Out-Null
$configureArgs.Add($buildDir) | Out-Null
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureArgs.Add("-G") | Out-Null
    $configureArgs.Add($Generator) | Out-Null
}

$buildArgs = [System.Collections.Generic.List[string]]::new()
$buildArgs.Add("--build") | Out-Null
$buildArgs.Add($buildDir) | Out-Null
$buildArgs.Add("--target") | Out-Null
$buildArgs.Add($targetName) | Out-Null
if ($Jobs -gt 0) {
    $buildArgs.Add("--parallel") | Out-Null
    $buildArgs.Add([Convert]::ToString($Jobs, [System.Globalization.CultureInfo]::InvariantCulture)) | Out-Null
}

Push-Location $repoRoot
try {
    Invoke-LoggedExternalTool `
        -Executable $cmake `
        -ArgumentList $configureArgs.ToArray() `
        -LogPath $configureLogPath `
        -FailureMessage "net lab route precedence configure failed" `
        -Phase "configure"

    Invoke-LoggedExternalTool `
        -Executable $cmake `
        -ArgumentList $buildArgs.ToArray() `
        -LogPath $buildLogPath `
        -FailureMessage "net lab route precedence build failed" `
        -Phase "build"

    $exePath = Resolve-ExecutablePath -BuildDir $buildDir -TargetName $targetName
    $runOutput = Invoke-LoggedProgram `
        -Executable $exePath `
        -LogPath $runLogPath `
        -FailureMessage "net lab route precedence run failed"
} finally {
    Pop-Location
}

$expectedLine = "net lab route precedence smoke: ok"
if (($runOutput -join "`n") -notmatch [regex]::Escape($expectedLine)) {
    throw "net lab route precedence smoke did not report success"
}

$checkLines = @(
    ("summary: {0}" -f $checkTextPath),
    "profile: net-lab-route-precedence-smoke",
    ("build_dir: {0}" -f $buildDir),
    ("executable: {0}" -f $exePath),
    ("expected_stdout: {0}" -f $expectedLine),
    "result: ok"
)
$checkLines | Set-Content -LiteralPath $checkTextPath -Encoding utf8

Write-Host ("[OK] {0}" -f $expectedLine)
Write-Host ("check={0}" -f $checkTextPath)
