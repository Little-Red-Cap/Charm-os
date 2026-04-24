param(
    [string]$Exe = "input-pump-win-demo"
)

Write-Host "Case 1: normal input"
& $Exe

Write-Host ""
Write-Host "Case 2: no input"
& $Exe --no-input
