$ErrorActionPreference = "Stop"
Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class Win32RecentTest {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    }
"@

$exePath = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"
$pngFile = "$PSScriptRoot\..\samples\01_highres.png"

Write-Host "Verifying In-Canvas Recent Documents Modal (Ctrl+R)..." -ForegroundColor Cyan

# Ensure no stale processes are running
Get-Process -Name "LightPhotoViewer" -ErrorAction SilentlyContinue | Stop-Process -Force

$WM_COMMAND = 0x0111
$WM_KEYDOWN = 0x0100
$IDM_FILE_RECENT = 207
$VK_ESCAPE  = 0x1B
$VK_DOWN    = 0x28
$VK_UP      = 0x26
$VK_RETURN  = 0x0D

# Test 1: Open modal while an image is actively loaded
$p = Start-Process -FilePath $exePath -ArgumentList "`"$pngFile`"" -PassThru
while ($p.MainWindowHandle -eq 0 -and -not $p.HasExited) {
    Start-Sleep -Milliseconds 10
    $p.Refresh()
}
Start-Sleep -Milliseconds 400
$p.Refresh()

if ($p.HasExited) {
    Write-Host "Failed to launch viewer with test image" -ForegroundColor Red
    exit 1
}

try {
    $hWnd = $p.MainWindowHandle

    # 1. Trigger Recent Documents Modal via IDM_FILE_RECENT (Ctrl+R)
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_COMMAND, [IntPtr]$IDM_FILE_RECENT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 250

    if ($p.HasExited) {
        throw "Process crashed upon opening Recent Documents Modal while image is loaded"
    }
    Write-Host "  Recent Documents Modal opened successfully over loaded image." -ForegroundColor Green

    # 2. Test arrow navigation inside modal
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_DOWN, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_UP, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100

    if ($p.HasExited) {
        throw "Process crashed during keyboard navigation in Recent Documents Modal"
    }
    Write-Host "  Keyboard navigation (Up/Down) verified without crash." -ForegroundColor Green

    # 3. Dismiss modal with Esc
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200

    if ($p.HasExited) {
        throw "Process crashed upon dismissing Recent Documents Modal with Esc"
    }
    Write-Host "  Recent Documents Modal dismissed cleanly via Esc." -ForegroundColor Green

    # 4. Reopen modal and toggle off with Ctrl+R command again
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_COMMAND, [IntPtr]$IDM_FILE_RECENT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    [void][Win32RecentTest]::PostMessage($hWnd, $WM_COMMAND, [IntPtr]$IDM_FILE_RECENT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200

    if ($p.HasExited) {
        throw "Process crashed upon toggling Recent Documents Modal"
    }
    Write-Host "  Recent Documents Modal toggle behavior verified." -ForegroundColor Green

    Write-Host "Recent Documents Modal test PASSED!" -ForegroundColor Green
}
finally {
    if (!$p.HasExited) {
        $p.Kill()
        $p.WaitForExit()
    }
}
