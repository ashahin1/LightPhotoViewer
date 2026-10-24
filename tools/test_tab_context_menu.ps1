$ErrorActionPreference = "Stop"

Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class Win32TabMenuTest {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    }
"@

$exePath = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"
$sample1 = "$PSScriptRoot\..\samples\01_highres.png"
$sample2 = "$PSScriptRoot\..\samples\02_sunset.jpg"
$sample3 = "$PSScriptRoot\..\samples\03_animated_spinner.gif"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Verifying Native Tab Strip Right-Click Context Menu     " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Run in-process Unit Tests (/test-tabs)
Write-Host "Running in-process Unit Tests (/test-tabs)..." -ForegroundColor Yellow
$proc = Start-Process -FilePath $exePath -ArgumentList "/test-tabs" -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Host "Tab unit tests failed with exit code $($proc.ExitCode)" -ForegroundColor Red
    if (Test-Path "$PSScriptRoot\..\test_tabs.log") {
        Get-Content "$PSScriptRoot\..\test_tabs.log"
    }
    exit 1
}

if (Test-Path "$PSScriptRoot\..\test_tabs.log") {
    Get-Content "$PSScriptRoot\..\test_tabs.log"
    Remove-Item "$PSScriptRoot\..\test_tabs.log" -Force
}
Write-Host "Unit tests passed successfully!" -ForegroundColor Green

# 2. Integration Test: Launch viewer with 3 tabs and verify window stability
Write-Host "Testing UI interaction with multiple tabs..." -ForegroundColor Yellow

# Ensure no stale processes
Get-Process -Name "LightPhotoViewer" -ErrorAction SilentlyContinue | Stop-Process -Force

$p = Start-Process -FilePath $exePath -ArgumentList "`"$sample1`" `"$sample2`" `"$sample3`"" -PassThru
while ($p.MainWindowHandle -eq 0 -and -not $p.HasExited) {
    Start-Sleep -Milliseconds 20
    $p.Refresh()
}
Start-Sleep -Milliseconds 400
$p.Refresh()

if ($p.HasExited) {
    Write-Host "Failed to launch viewer with 3 sample images" -ForegroundColor Red
    exit 1
}

try {
    $hWnd = $p.MainWindowHandle

    $WM_RBUTTONUP   = 0x0205
    $WM_CONTEXTMENU = 0x007B
    $WM_KEYDOWN     = 0x0100
    $VK_ESCAPE      = 0x001B

    # Right-click on Tab 0 (client coordinate x=50, y=18 within tab strip height 36)
    $tabLParam = [IntPtr]((18 -shl 16) -bor 50)
    [void][Win32TabMenuTest]::PostMessage($hWnd, $WM_RBUTTONUP, [IntPtr]::Zero, $tabLParam)
    Start-Sleep -Milliseconds 200

    if ($p.HasExited) {
        throw "Process crashed upon right-clicking tab header"
    }
    Write-Host "  Tab right-click context menu triggered without crashing." -ForegroundColor Green

    # Dismiss menu cleanly via Escape
    [void][Win32TabMenuTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100

    Write-Host "Tab Context Menu tests PASSED!" -ForegroundColor Green
}
finally {
    if (-not $p.HasExited) {
        $p.CloseMainWindow() | Out-Null
        Start-Sleep -Milliseconds 150
        if (-not $p.HasExited) {
            $p.Kill()
        }
    }
}
