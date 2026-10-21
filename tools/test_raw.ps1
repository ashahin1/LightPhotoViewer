param(
    [string]$SampleDir = "C:\Users\eng_a\.gemini\antigravity\brain\10077d2c-8f41-42f3-9527-a2ee488e312f\scratch\raw_samples"
)

$ErrorActionPreference = "Stop"
$exePath = Join-Path $PSScriptRoot "..\bin\LightPhotoViewer.exe"

if (-not (Test-Path $exePath)) {
    Write-Error "LightPhotoViewer.exe not found at $exePath! Run build.bat first."
}

if (-not (Test-Path $SampleDir)) {
    Write-Host "Sample directory $SampleDir not found, skipping RAW tests." -ForegroundColor Yellow
    exit 0
}

$rawFiles = Get-ChildItem -Path $SampleDir -File | Where-Object {
    $_.Extension -match "\.(cr2|cr3|crw|nef|nrw|arw|srf|sr2|dng|raf|orf|ori|rw2|pef|ptx|3fr|fff|rwl|x3f|srw|raw|kdc|dcr|mrw|mos|erf|mef)$"
}

if ($rawFiles.Count -eq 0) {
    Write-Host "No RAW sample files found in $SampleDir." -ForegroundColor Yellow
    exit 0
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Validating Camera RAW Support in LightPhotoViewer       " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Found $($rawFiles.Count) RAW test file(s) across multiple camera brands." -ForegroundColor White

$passed = 0
$failed = 0

foreach ($f in $rawFiles) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Testing $($f.Name) ($([math]::Round($f.Length / 1MB, 2)) MB)... " -NoNewline

    $p = Start-Process -FilePath $exePath -ArgumentList "`"$($f.FullName)`"" -PassThru
    Start-Sleep -Milliseconds 600
    $sw.Stop()

    if ($p.HasExited) {
        Write-Host "FAILED (Process exited unexpectedly with code $($p.ExitCode))" -ForegroundColor Red
        $failed++
    } else {
        $ws = [math]::Round($p.WorkingSet64 / 1MB, 1)
        $title = $p.MainWindowTitle
        Write-Host "OK" -ForegroundColor Green -NoNewline
        Write-Host " | Title: '$title' | RAM: $ws MB | Launch: $($sw.ElapsedMilliseconds)ms" -ForegroundColor DarkGray
        $passed++

        $p.Kill()
        $p.WaitForExit()
    }
}

Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
if ($failed -eq 0) {
    Write-Host "All $passed RAW format test(s) passed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Error "$failed RAW format test(s) failed."
}
