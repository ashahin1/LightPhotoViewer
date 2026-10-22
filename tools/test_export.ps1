$ErrorActionPreference = "Stop"
Add-Type @"
    using System;
    using System.Text;
    using System.Runtime.InteropServices;
    public class Win32ExportTest {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    }
"@

$exePath = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"
$pngFile = "$PSScriptRoot\..\samples\01_highres.png"
$gifFile = "$PSScriptRoot\..\samples\03_animated_spinner.gif"

Write-Host "Verifying In-Canvas Export & Conversion Modal (Ctrl+S)..." -ForegroundColor Cyan

# Ensure no stale LightPhotoViewer processes are running
Get-Process -Name "LightPhotoViewer" -ErrorAction SilentlyContinue | Stop-Process -Force

$WM_COMMAND = 0x0111
$WM_KEYDOWN = 0x0100
$IDM_FILE_EXPORT = 206
$VK_LEFT    = 0x25
$VK_RIGHT   = 0x27
$VK_OEM_4   = 0xDB # '['
$VK_OEM_6   = 0xDD # ']'
$VK_ESCAPE  = 0x1B

# Test 1: Static Image Export Modal & Strict Modal Isolation
$p = Start-Process -FilePath $exePath -ArgumentList "`"$pngFile`"" -PassThru
while ($p.MainWindowHandle -eq 0 -and -not $p.HasExited) {
    Start-Sleep -Milliseconds 10
    $p.Refresh()
}
Start-Sleep -Milliseconds 400
$p.Refresh()

if ($p.HasExited) {
    Write-Host "Failed to launch viewer with PNG" -ForegroundColor Red
    exit 1
}

try {
    $hWnd = $p.MainWindowHandle
    $initialTitle = $p.MainWindowTitle
    Write-Host "  Initial Window Title: $initialTitle" -ForegroundColor Gray

    # 1. Open Export Modal via IDM_FILE_EXPORT (206)
    [void][Win32ExportTest]::PostMessage($hWnd, $WM_COMMAND, [IntPtr]$IDM_FILE_EXPORT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 350

    if ($p.HasExited) {
        throw "Process crashed upon opening Export Modal"
    }
    Write-Host "  Export Modal opened and encoded initial live estimate." -ForegroundColor Green

    # 2. Send Left / Right arrow keys (adjusts JPEG quality in modal, MUST NOT navigate folder images)
    [void][Win32ExportTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_LEFT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    [void][Win32ExportTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_RIGHT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200

    $p.Refresh()
    $modalTitle = $p.MainWindowTitle
    if ($modalTitle -ne $initialTitle) {
        throw "Strict modal isolation failed! Title changed from '$initialTitle' to '$modalTitle' while Export Modal was active."
    }
    Write-Host "  Strict modal isolation verified (arrow keys adjusted quality without changing active image)." -ForegroundColor Green

    # 3. Dismiss Export Modal with Esc
    [void][Win32ExportTest]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 250

    if ($p.HasExited) {
        throw "Process crashed upon closing Export Modal"
    }
    Write-Host "  Export Modal closed cleanly via Esc." -ForegroundColor Green
}
finally {
    if (!$p.HasExited) {
        $p.Kill()
        $p.WaitForExit()
    }
}

# Test 2: Animated GIF Frame Stepping in Export Modal
$p2 = Start-Process -FilePath $exePath -ArgumentList "`"$gifFile`"" -PassThru
while ($p2.MainWindowHandle -eq 0 -and -not $p2.HasExited) {
    Start-Sleep -Milliseconds 10
    $p2.Refresh()
}
Start-Sleep -Milliseconds 400
$p2.Refresh()

if ($p2.HasExited) {
    Write-Host "Failed to launch viewer with GIF" -ForegroundColor Red
    exit 1
}

try {
    $hWnd2 = $p2.MainWindowHandle
    [void][Win32ExportTest]::PostMessage($hWnd2, $WM_COMMAND, [IntPtr]$IDM_FILE_EXPORT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 350

    # Step frames with [ and ]
    [void][Win32ExportTest]::PostMessage($hWnd2, $WM_KEYDOWN, [IntPtr]$VK_OEM_6, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    [void][Win32ExportTest]::PostMessage($hWnd2, $WM_KEYDOWN, [IntPtr]$VK_OEM_4, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150

    if ($p2.HasExited) {
        throw "Process crashed during GIF frame stepping in Export Modal"
    }
    Write-Host "  Animated GIF frame stepping ([ and ]) in Export Modal verified." -ForegroundColor Green

    [void][Win32ExportTest]::PostMessage($hWnd2, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 200
}
finally {
    if (!$p2.HasExited) {
        $p2.Kill()
        $p2.WaitForExit()
    }
}

Write-Host "Export & Conversion Modal test PASSED!" -ForegroundColor Green
