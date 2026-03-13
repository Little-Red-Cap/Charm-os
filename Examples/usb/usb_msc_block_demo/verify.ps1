$ErrorActionPreference = "Stop"

param(
    [string]$Image = ""
)

function Run-Case {
    param(
        [string]$Label,
        [string[]]$Args
    )
    Write-Host "[case] $Label"
    & .\usb-msc-block-demo @Args
    Write-Host ""
}

Run-Case "missing image" @("missing.img")

if ($Image -ne "") {
    Run-Case "valid image" @($Image)
} else {
    Write-Host "[skip] no image provided; pass -Image <path>"
}
