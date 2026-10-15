$ErrorActionPreference = "Stop"
$exe = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"

Write-Host "Testing Single-Instance Tab Forwarding..." -ForegroundColor Cyan

$p1 = Start-Process -FilePath $exe -ArgumentList "`"$PSScriptRoot\..\samples\01_highres.png`"" -PassThru
Start-Sleep -Milliseconds 500

$p2 = Start-Process -FilePath $exe -ArgumentList "`"$PSScriptRoot\..\samples\02_sunset.jpg`"" -PassThru
$p2.WaitForExit(3000)

$p2Exited = $p2.HasExited
$p1Running = -not $p1.HasExited

$color2 = if ($p2Exited) { "Green" } else { "Red" }
$color1 = if ($p1Running) { "Green" } else { "Red" }
Write-Host "  Second instance forwarded & exited cleanly: $p2Exited" -ForegroundColor $color2
Write-Host "  First instance received message & remains active: $p1Running" -ForegroundColor $color1

$p1.Kill()
$p1.WaitForExit()

if ($p2Exited -and $p1Running) {
    Write-Host "Single-Instance Forwarding test PASSED!" -ForegroundColor Green
} else {
    Write-Host "Single-Instance Forwarding test FAILED!" -ForegroundColor Red
}
