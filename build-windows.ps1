param(
    [string]$QtBin = "",
    [ValidateSet("release", "debug")]
    [string]$Configuration = "release",
    [string]$OutDir = ".\release\windows"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Project = Join-Path $Root "MH4GSaveEditor.pro"
$Target = "MH4GSaveEditor"

if (-not $QtBin) {
    if ($env:QTDIR) {
        $QtBin = Join-Path $env:QTDIR "bin"
    } elseif (Test-Path "C:\msys64\mingw64\bin") {
        $QtBin = "C:\msys64\mingw64\bin"
    } else {
        throw "Qt MinGW bin directory not found. Pass -QtBin."
    }
}

$QtBin = (Resolve-Path $QtBin).Path
$Qmake = @("qmake.exe", "qmake-qt5.exe") |
    ForEach-Object { Join-Path $QtBin $_ } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
$Make = Join-Path $QtBin "mingw32-make.exe"
$Deploy = @("windeployqt.exe", "windeployqt-qt5.exe") |
    ForEach-Object { Join-Path $QtBin $_ } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1

if (-not $Qmake) { throw "qmake was not found in $QtBin" }
if (-not (Test-Path $Make)) { throw "mingw32-make.exe was not found in $QtBin" }
if (-not $Deploy) { throw "windeployqt was not found in $QtBin" }

Set-Location $Root
$env:PATH = "$QtBin;$env:PATH"

foreach ($path in @("Makefile", "Makefile.Debug", "Makefile.Release", "build", "bin")) {
    $full = Join-Path $Root $path
    if (Test-Path $full) { Remove-Item $full -Recurse -Force }
}

& $Qmake $Project "CONFIG+=$Configuration" "CONFIG-=debug_and_release"
& $Make "-j$([Environment]::ProcessorCount)"

$BuiltExe = Join-Path $Root "bin\$Target.exe"
if (-not (Test-Path $BuiltExe)) { throw "Executable not found: $BuiltExe" }

$Package = Join-Path $Root $OutDir
if (Test-Path $Package) { Remove-Item $Package -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Package | Out-Null
Copy-Item $BuiltExe (Join-Path $Package "$Target.exe")
Copy-Item (Join-Path $Root "data") (Join-Path $Package "data") -Recurse

& $Deploy (Join-Path $Package "$Target.exe") "--$Configuration"

$runtimeDlls = @(
    "libcrypto-3-x64.dll",
    "libcrypto-3.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)
foreach ($dll in $runtimeDlls) {
    $source = Join-Path $QtBin $dll
    if (Test-Path $source) { Copy-Item $source (Join-Path $Package $dll) -Force }
}

Set-Content -Path (Join-Path $Package "run-windows.bat") -Encoding ASCII -Value @"
@echo off
cd /d "%~dp0"
start "" "$Target.exe"
"@

Write-Host "Windows package created: $Package"
