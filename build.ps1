param(
    [switch]$Rebuild,
    [switch]$Clean,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Building LightPhotoViewer (Ultra-Fast Image Viewer)     " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Handle Clean
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path "bin\obj") { Remove-Item -Path "bin\obj" -Recurse -Force }
    if (Test-Path "bin\LightPhotoViewer.exe") { Remove-Item -Path "bin\LightPhotoViewer.exe" -Force }
    if (Test-Path "resources\app.res") { Remove-Item -Path "resources\app.res" -Force }
    Write-Host "Clean completed successfully." -ForegroundColor Green
    exit 0
}

# 2. Handle Rebuild
if ($Rebuild) {
    Write-Host "Full rebuild requested. Purging bin\obj..." -ForegroundColor Yellow
    if (Test-Path "bin\obj") { Remove-Item -Path "bin\obj" -Recurse -Force }
    if (Test-Path "resources\app.res") { Remove-Item -Path "resources\app.res" -Force }
}

# 3. Ensure required directories
if (-not (Test-Path "bin")) { New-Item -ItemType Directory -Path "bin" | Out-Null }
if (-not (Test-Path "bin\obj")) { New-Item -ItemType Directory -Path "bin\obj" -Force | Out-Null }

# 4. Activate MSVC
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $msvcCandidate = $null
    if (Test-Path "tools\msvc\activate.ps1") {
        $msvcCandidate = "$PSScriptRoot\tools\msvc\activate.ps1"
    } elseif (Test-Path "$PSScriptRoot\..\LightPDF\tools\msvc\activate.ps1") {
        $msvcCandidate = "$PSScriptRoot\..\LightPDF\tools\msvc\activate.ps1"
    }

    if ($msvcCandidate) {
        Write-Host "Activating portable MSVC from $msvcCandidate..." -ForegroundColor Yellow
        . $msvcCandidate
    } else {
        # Check standard Visual Studio installation via vswhere
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $activated = $false
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) {
                $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
                if (Test-Path $vcvars) {
                    Write-Host "Activating Visual Studio environment via $vcvars..." -ForegroundColor Yellow
                    & cmd.exe /c "call `"$vcvars`" && set" | ForEach-Object {
                        if ($_ -match "=(.*)") {
                            $var = $_.Substring(0, $_.IndexOf('='))
                            $val = $_.Substring($_.IndexOf('=') + 1)
                            [System.Environment]::SetEnvironmentVariable($var, $val)
                        }
                    }
                    $activated = $true
                }
            }
        }
        if (-not $activated -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            Write-Error "cl.exe not found and neither portable MSVC nor Visual Studio installation was detected!"
        }
    }
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

# 5. Compile Resource Script (app.rc)
$rcInputs = @(
    "resources\app.rc",
    "resources\app.ico"
)
$resFile = "resources\app.res"
$needRc = -not (Test-Path $resFile)

if (-not $needRc) {
    $resTime = (Get-Item $resFile).LastWriteTimeUtc
    foreach ($rcIn in $rcInputs) {
        if ((Test-Path $rcIn) -and (Get-Item $rcIn).LastWriteTimeUtc -gt $resTime) {
            $needRc = $true
            break
        }
    }
}

if ($needRc) {
    Write-Host "Compiling Windows resource script (app.rc)..." -ForegroundColor Green
    & rc.exe /nologo /i resources /fo "resources\app.res" "resources\app.rc"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Resource compilation failed with exit code $LASTEXITCODE"
    }
}

# 6. Precompiled Header (PCH) Management
$pchHpp = "src\pch.hpp"
$pchCpp = "src\pch.cpp"
$pchBin = "bin\obj\pch.pch"
$pchObj = "bin\obj\pch.obj"
$pchRebuilt = $false

$needPch = (-not (Test-Path $pchBin)) -or (-not (Test-Path $pchObj))
if (-not $needPch) {
    $pchTime = (Get-Item $pchBin).LastWriteTimeUtc
    if ((Get-Item $pchHpp).LastWriteTimeUtc -gt $pchTime -or (Get-Item $pchCpp).LastWriteTimeUtc -gt $pchTime) {
        $needPch = $true
    }
}

$commonFlags = @(
    "/nologo",
    "/std:c++20",
    "/W4",
    "/EHsc",
    "/permissive-",
    "/utf-8",
    "/DNOMINMAX",
    "/DUNICODE",
    "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN"
)

if ($Debug) {
    $optFlags = @("/Od", "/MTd", "/Zi", "/D_DEBUG")
} else {
    $optFlags = @("/O1", "/MT", "/GL", "/Gy", "/Gw", "/DNDEBUG")
}

if ($needPch) {
    Write-Host "Generating Precompiled Header (pch.pch)..." -ForegroundColor Yellow
    $pchArgs = $commonFlags + $optFlags + @(
        "/Yc`"pch.hpp`"",
        "/Fp`"$pchBin`"",
        "/Fo`"$pchObj`"",
        "/c",
        $pchCpp
    )
    & cl.exe @pchArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "PCH generation failed with exit code $LASTEXITCODE"
    }
    $pchRebuilt = $true
}

# 7. Compile Source Files (.cpp)
$cppFiles = Get-ChildItem -Path "src" -Filter "*.cpp" | Where-Object { $_.Name -ne "pch.cpp" }
$objsToLink = @($pchObj)
$cppToCompile = @()

$hppFiles = Get-ChildItem -Path "src" -Filter "*.hpp"
$latestHppTime = if ($hppFiles) { ($hppFiles | Measure-Object -Property LastWriteTimeUtc -Maximum).Maximum } else { [DateTime]::MinValue }

foreach ($f in $cppFiles) {
    $objFile = "bin\obj\$($f.BaseName).obj"
    $needCompile = $pchRebuilt -or (-not (Test-Path $objFile))
    if (-not $needCompile) {
        $objTime = (Get-Item $objFile).LastWriteTimeUtc
        if ($f.LastWriteTimeUtc -gt $objTime -or $latestHppTime -gt $objTime) {
            $needCompile = $true
        }
    }

    if ($needCompile) {
        $cppToCompile += $f.FullName
    }
    $objsToLink += $objFile
}

if ($cppToCompile.Count -gt 0) {
    Write-Host "Compiling $($cppToCompile.Count) source file(s)..." -ForegroundColor Green
    $clArgs = $commonFlags + $optFlags + @(
        "/MP",
        "/Yu`"pch.hpp`"",
        "/FI`"pch.hpp`"",
        "/Fp`"$pchBin`"",
        "/Fo`"bin\obj/`"",
        "/I", "src",
        "/c"
    ) + $cppToCompile

    & cl.exe $clArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Host "All source objects up to date." -ForegroundColor DarkGray
}

# 8. Link Executable
$exeFile = "bin\LightPhotoViewer.exe"
$needLink = (-not (Test-Path $exeFile)) -or ($cppToCompile.Count -gt 0) -or $needRc

if (-not $needLink) {
    $exeTime = (Get-Item $exeFile).LastWriteTimeUtc
    foreach ($obj in $objsToLink) {
        if ((Test-Path $obj) -and (Get-Item $obj).LastWriteTimeUtc -gt $exeTime) {
            $needLink = $true
            break
        }
    }
}

if ($needLink) {
    Write-Host "Linking LightPhotoViewer.exe..." -ForegroundColor Yellow
    $libs = @(
        "user32.lib",
        "kernel32.lib",
        "gdi32.lib",
        "comctl32.lib",
        "comdlg32.lib",
        "winspool.lib",
        "shell32.lib",
        "shlwapi.lib",
        "dwmapi.lib",
        "uxtheme.lib",
        "windowscodecs.lib",
        "d3d11.lib",
        "d2d1.lib",
        "dwrite.lib",
        "dxgi.lib",
        "ole32.lib",
        "oleaut32.lib",
        "uuid.lib",
        "advapi32.lib"
    )

    $linkFlags = @(
        "/NOLOGO",
        "/SUBSYSTEM:WINDOWS",
        "/OUT:`"$exeFile`"",
        "/MANIFEST:EMBED",
        "/MANIFESTINPUT:`"resources\app.manifest`""
    )

    if (-not $Debug) {
        $linkFlags += @("/LTCG", "/OPT:REF", "/OPT:ICF")
    }

    $linkArgs = $objsToLink + @("resources\app.res") + $libs + $linkFlags
    & link.exe @linkArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Linking failed with exit code $LASTEXITCODE"
    }

    $sw.Stop()
    $sizeKb = [math]::Round((Get-Item $exeFile).Length / 1KB, 1)
    Write-Host "Build successful! Created $exeFile ($sizeKb KB) in $([math]::Round($sw.Elapsed.TotalSeconds, 2))s" -ForegroundColor Green
} else {
    $sw.Stop()
    Write-Host "Executable is up to date." -ForegroundColor Green
}
