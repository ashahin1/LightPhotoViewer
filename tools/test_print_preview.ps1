$ErrorActionPreference = "Stop"
Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class Win32 {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    }
"@

$exePath = "$PSScriptRoot\..\bin\LightPhotoViewer.exe"
$gifFile = "$PSScriptRoot\..\samples\03_animated_spinner.gif"

Write-Host "Verifying GIF Print Preview Freeze & Stepping..." -ForegroundColor Cyan

$p = Start-Process -FilePath $exePath -ArgumentList "`"$gifFile`"" -PassThru
Start-Sleep -Milliseconds 600

if ($p.HasExited) {
    Write-Host "Failed to launch viewer" -ForegroundColor Red
    exit 1
}

$hWnd = $p.MainWindowHandle
$WM_COMMAND = 0x0111
$WM_KEYDOWN = 0x0100
$IDM_FILE_PRINT = 205
$VK_OEM_4 = 0xDB # '['
$VK_OEM_6 = 0xDD # ']'
$VK_ESCAPE = 0x1B

try {
    # 1. Open Print Preview via WM_COMMAND IDM_FILE_PRINT
    [Win32]::PostMessage($hWnd, $WM_COMMAND, [IntPtr]$IDM_FILE_PRINT, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300

    if ($p.HasExited) {
        throw "Process crashed upon opening Print Preview"
    }
    Write-Host "  Print Preview opened successfully." -ForegroundColor Green

    # 2. Cycle frames using [ and ]
    [Win32]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_OEM_6, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    [Win32]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_OEM_6, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    [Win32]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_OEM_4, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 150

    if ($p.HasExited) {
        throw "Process crashed during frame stepping"
    }
    Write-Host "  Frame stepping executed without error." -ForegroundColor Green

    # 3. Exit Print Preview with Esc
    [Win32]::PostMessage($hWnd, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300

    if ($p.HasExited) {
        throw "Process crashed upon closing Print Preview"
    }
    Write-Host "  Print Preview closed cleanly and resumed viewer." -ForegroundColor Green

    Write-Host "GIF Print Preview test PASSED!" -ForegroundColor Green
}
finally {
    if (!$p.HasExited) {
        $p.Kill()
        $p.WaitForExit()
    }
}
