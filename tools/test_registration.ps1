$ErrorActionPreference = "Stop"
$exe = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"

Write-Host "Testing Shell Associations (/register and /unregister)..." -ForegroundColor Cyan

# 1. Register
$pReg = Start-Process -FilePath $exe -ArgumentList "/register /silent" -Wait -PassThru
$regProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.jpg\OpenWithProgids" -ErrorAction SilentlyContinue
$isReg = ($null -ne $regProp) -and ($null -ne $regProp."LightPhotoViewer.Image")

$c1 = if ($isReg) { "Green" } else { "Red" }
Write-Host "  Registered .jpg extension: $isReg" -ForegroundColor $c1

# 2. Unregister
$pUnreg = Start-Process -FilePath $exe -ArgumentList "/unregister /silent" -Wait -PassThru
$unregProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.jpg\OpenWithProgids" -ErrorAction SilentlyContinue
$isUnreg = ($null -eq $unregProp) -or ($null -eq $unregProp."LightPhotoViewer.Image")

$c2 = if ($isUnreg) { "Green" } else { "Red" }
Write-Host "  Unregistered .jpg cleanly: $isUnreg" -ForegroundColor $c2

if ($isReg -and $isUnreg) {
    Write-Host "Shell Association test PASSED!" -ForegroundColor Green
} else {
    Write-Host "Shell Association test FAILED!" -ForegroundColor Red
}
