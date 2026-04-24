param(
    [string]$Exe = "fs-block-vfs-demo",
    [string]$Missing = "notfound.img",
    [string]$Invalid = "invalid_mbr.img",
    [string]$NoFat = "no_fat.img",
    [string]$Valid = "disk.img"
)

Write-Host "Case 1: image missing"
& $Exe $Missing

Write-Host ""
Write-Host "Case 2: invalid MBR"
& $Exe $Invalid

Write-Host ""
Write-Host "Case 3: no FAT partition"
& $Exe $NoFat

Write-Host ""
Write-Host "Case 4: valid FAT image"
& $Exe $Valid
