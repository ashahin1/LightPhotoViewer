$ErrorActionPreference = "Stop"
$exe = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"

Write-Host "Testing Shell Associations (/register and /unregister)..." -ForegroundColor Cyan

# 1. Register
$pReg = Start-Process -FilePath $exe -ArgumentList "/register /silent" -Wait -PassThru
$regProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.jpg\OpenWithProgids" -ErrorAction SilentlyContinue
$nefProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.nef\OpenWithProgids" -ErrorAction SilentlyContinue
$heicProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.heic\OpenWithProgids" -ErrorAction SilentlyContinue
$appKey = Test-Path "HKCU:\Software\Classes\Applications\LightPhotoViewer.exe\shell\open\command"
$regApp = Get-ItemProperty -Path "HKCU:\Software\RegisteredApplications" -ErrorAction SilentlyContinue
$isReg = ($null -ne $regProp) -and ($null -ne $regProp."LightPhotoViewer.Image") -and ($null -ne $nefProp."LightPhotoViewer.Image") -and ($null -ne $heicProp."LightPhotoViewer.Image") -and $appKey -and ($null -ne $regApp.LightPhotoViewer)

$c1 = if ($isReg) { "Green" } else { "Red" }
Write-Host "  Registered .jpg extension: $isReg" -ForegroundColor $c1

# 2. Unregister
$pUnreg = Start-Process -FilePath $exe -ArgumentList "/unregister /silent" -Wait -PassThru
$unregProp = Get-ItemProperty -Path "HKCU:\Software\Classes\.jpg\OpenWithProgids" -ErrorAction SilentlyContinue
$regApp2 = Get-ItemProperty -Path "HKCU:\Software\RegisteredApplications" -ErrorAction SilentlyContinue
$isUnreg = (($null -eq $unregProp) -or ($null -eq $unregProp."LightPhotoViewer.Image")) -and -not (Test-Path "HKCU:\Software\Classes\Applications\LightPhotoViewer.exe") -and -not (Test-Path "HKCU:\Software\LightPhotoViewer") -and ($null -eq $regApp2.LightPhotoViewer)

$c2 = if ($isUnreg) { "Green" } else { "Red" }
Write-Host "  Unregistered .jpg cleanly: $isUnreg" -ForegroundColor $c2

if ($isReg -and $isUnreg) {
    Write-Host "Shell Association test PASSED!" -ForegroundColor Green
} else {
    Write-Host "Shell Association test FAILED!" -ForegroundColor Red
}

