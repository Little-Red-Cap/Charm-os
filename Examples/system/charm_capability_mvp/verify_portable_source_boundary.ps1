[CmdletBinding()]
param(
    [ValidateSet("host")]
    [string[]]$Domains = @("host"),
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

$PlatformTokenPattern = '(?i)(stm32|h747|qemu|win32|freertos|threadx|zephyr|cmsis|provider_identity|\b(linux|uart|dma|vendor)\b)'
$ConditionalPattern = '(?m)^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b'

function Assert-PortableSharedText {
    param(
        [string]$Label,
        [string]$Text
    )

    $PlatformMatch = [System.Text.RegularExpressions.Regex]::Match(
        $Text,
        $PlatformTokenPattern)
    if ($PlatformMatch.Success) {
        throw "platform_token_forbidden: $Label token=$($PlatformMatch.Value)"
    }

    $ConditionalMatch = [System.Text.RegularExpressions.Regex]::Match(
        $Text,
        $ConditionalPattern)
    if ($ConditionalMatch.Success) {
        throw "platform_conditional_forbidden: $Label directive=$($ConditionalMatch.Value.Trim())"
    }
}

function Assert-SinglePortableAppInclude {
    param(
        [string]$Label,
        [string]$Text
    )

    $IncludePattern = '(?m)^\s*#\s*include\s+"mvp_app\.hpp"\s*$'
    $Matches = [System.Text.RegularExpressions.Regex]::Matches($Text, $IncludePattern)
    if ($Matches.Count -ne 1) {
        throw "portable_app_include_mismatch: $Label count=$($Matches.Count)"
    }
    if ($Text.Contains('namespace charm::mvp::app')) {
        throw "portable_app_redefinition: $Label"
    }
}

function Assert-Rejected {
    param(
        [scriptblock]$Action,
        [string]$ExpectedPrefix
    )

    $Rejected = $false
    try {
        & $Action
    } catch {
        if (-not $_.Exception.Message.StartsWith($ExpectedPrefix)) {
            throw
        }
        $Rejected = $true
    }
    if (-not $Rejected) {
        throw "self_test_failed: expected rejection '$ExpectedPrefix'"
    }
}

if ($SelfTest) {
    Assert-PortableSharedText -Label "good" -Text @"
#pragma once
#include <cstdint>
namespace sample { struct Clock {}; }
"@
    Assert-Rejected `
        -ExpectedPrefix "platform_token_forbidden:" `
        -Action { Assert-PortableSharedText -Label "bad-token" -Text "struct STM32Clock {};" }
    Assert-Rejected `
        -ExpectedPrefix "platform_conditional_forbidden:" `
        -Action { Assert-PortableSharedText -Label "bad-conditional" -Text "#ifdef TARGET`n#endif" }
    Assert-SinglePortableAppInclude `
        -Label "good-consumer" `
        -Text "#include `"mvp_app.hpp`"`nint main();"
    Assert-Rejected `
        -ExpectedPrefix "portable_app_include_mismatch:" `
        -Action { Assert-SinglePortableAppInclude -Label "bad-consumer" -Text "int main();" }
    Write-Output "[charm-capability-mvp-source-boundary-self-test] ok"
    exit 0
}

$CharmRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..\..")).Path
$SharedFiles = @(
    (Join-Path $PSScriptRoot "mvp_contracts.hpp"),
    (Join-Path $PSScriptRoot "mvp_composition.hpp"),
    (Join-Path $PSScriptRoot "mvp_app.hpp")
)
$Consumers = [ordered]@{
    host = Join-Path $PSScriptRoot "main.cpp"
}

if ($Domains.Count -eq 0) {
    throw "domains_missing"
}

foreach ($Path in $SharedFiles) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "shared_source_missing: $Path"
    }
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    Assert-PortableSharedText -Label ([System.IO.Path]::GetFileName($Path)) -Text $Text
    $Hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Output "[charm-capability-mvp-source-boundary] shared=$([System.IO.Path]::GetFileName($Path)) sha256=$Hash"
}

foreach ($Domain in $Domains) {
    $Path = $Consumers[$Domain]
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "consumer_source_missing: $Domain path=$Path"
    }
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    Assert-SinglePortableAppInclude -Label $Domain -Text $Text
    Write-Output "[charm-capability-mvp-source-boundary] consumer=$Domain app=mvp_app.hpp"
}

Write-Output "[charm-capability-mvp-source-boundary] ok domains=$($Domains -join ',')"
