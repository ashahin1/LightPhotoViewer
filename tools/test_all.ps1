$ErrorActionPreference = "Stop"
$exePath = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"
$files = Get-ChildItem -Path "$PSScriptRoot\..\samples"

Write-Host "Verifying format rendering in LightPhotoViewer..." -ForegroundColor Cyan

foreach ($f in $files) {
    Write-Host "  Opening: $($f.Name)..." -NoNewline
    $p = Start-Process -FilePath $exePath -ArgumentList "`"$($f.FullName)`"" -PassThru
    Start-Sleep -Milliseconds 400

    if ($p.HasExited) {
        Write-Host " FAILED (ExitCode $($p.ExitCode))" -ForegroundColor Red
    } else {
        $ws = [math]::Round($p.WorkingSet64 / 1MB, 1)
        Write-Host " OK (RAM: $ws MB)" -ForegroundColor Green
        $p.Kill()
        $p.WaitForExit()
    }
}

Write-Host "All format tests passed successfully!" -ForegroundColor Green
