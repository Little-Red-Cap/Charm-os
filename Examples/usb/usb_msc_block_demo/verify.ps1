param(
    [string]$Image = "",
    [string]$Exe = "",
    [int]$TimeoutSec = 5
)

$ErrorActionPreference = "Stop"

if ($Exe -eq "") {
    $Exe = "C:\\Charm-os-build\\usb_msc_block_demo\\Debug\\usb-msc-block-demo.exe"
}

if (-not (Test-Path $Exe)) {
    throw "demo not found: $Exe"
}

function Run-Case {
    param(
        [string]$Label,
        [string[]]$CaseArgs
    )
    Write-Host "[case] $Label"
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    $proc = Start-Process -FilePath $Exe -ArgumentList $CaseArgs -PassThru -NoNewWindow `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $exited = $proc.WaitForExit($TimeoutSec * 1000)
    if (-not $exited) {
        $proc | Stop-Process
        Write-Host "[WARN] timeout after ${TimeoutSec}s"
    }
    if (Test-Path $outFile) {
        Get-Content $outFile
        Remove-Item $outFile -Force
    }
    if (Test-Path $errFile) {
        $err = Get-Content $errFile
        if ($err) { $err }
        Remove-Item $errFile -Force
    }
    Write-Host ""
}

Run-Case "missing image" @("missing.img")

if ($Image -ne "") {
    Run-Case "valid image" @($Image)
} else {
    Write-Host "[skip] no image provided; pass -Image <path>"
}
