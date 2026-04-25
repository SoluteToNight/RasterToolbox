param(
    [ValidateSet("configure", "build", "deploy", "test", "smoke", "run", "all")]
    [string]$Action = "all"
)

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;

namespace Win32 {
    public static class NativeMethods {
        [DllImport("kernel32.dll")]
        public static extern uint SetErrorMode(uint uMode);
    }
}
"@

$SEM_FAILCRITICALERRORS = 0x0001
$SEM_NOGPFAULTERRORBOX = 0x0002
$SEM_NOOPENFILEERRORBOX = 0x8000
[Win32.NativeMethods]::SetErrorMode($SEM_FAILCRITICALERRORS -bor $SEM_NOGPFAULTERRORBOX -bor $SEM_NOOPENFILEERRORBOX) | Out-Null

$repoRoot = Split-Path -Parent $PSScriptRoot
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmakeExe = "C:\Program Files\CMake\bin\cmake.exe"
$ctestExe = "C:\Program Files\CMake\bin\ctest.exe"
$vcpkgRoot = "E:\vcpkg"
$vcpkgInstalledDir = "E:\vcpkg\installed\x64-windows"
$vcpkgBinDir = Join-Path $vcpkgInstalledDir "bin"
$ninjaDir = "C:\Users\DELL\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
$ninjaExe = Join-Path $ninjaDir "ninja.exe"
$sevenZipDir = "E:\vcpkg\downloads\tools\7zip-26.00-windows"
$sevenZrDir = "E:\vcpkg\downloads\tools\7zr-26.00-windows"
$sdkBinDir = "$repoRoot\.cache\winsdk-bin"
$sdkLibUcrtDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
$sdkLibUmDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
$sdkIncludeUcrtDir = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt"
$sdkIncludeUmDir = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um"
$sdkIncludeSharedDir = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared"
$sdkIncludeWinRTDir = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\winrt"
$sdkIncludeCppWinRTDir = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt"
$qtBinDir = "D:\Qt\6.11.0\msvc2022_64\bin"
$qtPluginsDir = "D:\Qt\6.11.0\msvc2022_64\plugins"
$gdalDataDir = Join-Path $vcpkgInstalledDir "share\gdal"
$projDataDir = Join-Path $vcpkgInstalledDir "share\proj"
$preset = "local-msvc-vcpkg"
$buildDir = Join-Path $repoRoot "build\local-msvc-vcpkg-clean"
$exePath = Join-Path $buildDir "rastertoolbox.exe"
$baseSystemPath = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem;$env:SystemRoot\System32\WindowsPowerShell\v1.0;$env:SystemRoot\System32\OpenSSH"

foreach ($path in @(
    $vcvars,
    $cmakeExe,
    $ctestExe,
    $vcpkgRoot,
    $vcpkgBinDir,
    $ninjaDir,
    $ninjaExe,
    $sevenZipDir,
    $sevenZrDir,
    $sdkBinDir,
    $sdkLibUcrtDir,
    $sdkLibUmDir,
    $sdkIncludeUcrtDir,
    $sdkIncludeUmDir,
    $sdkIncludeSharedDir,
    $sdkIncludeWinRTDir,
    $sdkIncludeCppWinRTDir,
    $qtBinDir,
    $qtPluginsDir
)) {
    if (-not (Test-Path $path)) {
        throw "Missing required path: $path"
    }
}

function Get-ConfigureCommands {
    if (Test-Path (Join-Path $buildDir "CMakeCache.txt")) {
        return @()
    }

    return @("`"$cmakeExe`" --preset $preset")
}

function Invoke-LocalDeploy {
    if (-not (Test-Path $exePath)) {
        throw "Missing executable: $exePath"
    }

    foreach ($path in @($vcpkgBinDir, $gdalDataDir, $projDataDir)) {
        if (-not (Test-Path $path)) {
            throw "Missing required runtime path: $path"
        }
    }

    foreach ($qtDllName in @(
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Widgets.dll",
        "Qt6Concurrent.dll",
        "d3dcompiler_47.dll",
        "opengl32sw.dll"
    )) {
        $qtDllPath = Join-Path $qtBinDir $qtDllName
        if (Test-Path $qtDllPath) {
            Copy-Item -Path $qtDllPath -Destination (Join-Path $buildDir $qtDllName) -Force
        }
    }

    foreach ($pluginSubdir in @("platforms", "styles", "imageformats", "iconengines")) {
        $sourcePluginDir = Join-Path $qtPluginsDir $pluginSubdir
        $targetPluginDir = Join-Path $buildDir $pluginSubdir

        if (-not (Test-Path $sourcePluginDir)) {
            continue
        }

        if (Test-Path $targetPluginDir) {
            Remove-Item -Path $targetPluginDir -Recurse -Force
        }

        New-Item -Path $targetPluginDir -ItemType Directory -Force | Out-Null
        Get-ChildItem -Path $sourcePluginDir -Filter '*.dll' -File | ForEach-Object {
            Copy-Item -Path $_.FullName -Destination (Join-Path $targetPluginDir $_.Name) -Force
        }
    }

    Get-ChildItem -Path $vcpkgBinDir -Filter '*.dll' -File | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination (Join-Path $buildDir $_.Name) -Force
    }

    $shareDir = Join-Path $buildDir "share"
    $buildGdalDataDir = Join-Path $shareDir "gdal"
    $buildProjDataDir = Join-Path $shareDir "proj"

    New-Item -Path $shareDir -ItemType Directory -Force | Out-Null
    if (Test-Path $buildGdalDataDir) {
        Remove-Item -Path $buildGdalDataDir -Recurse -Force
    }
    if (Test-Path $buildProjDataDir) {
        Remove-Item -Path $buildProjDataDir -Recurse -Force
    }
    Copy-Item -Path $gdalDataDir -Destination $buildGdalDataDir -Recurse -Force
    Copy-Item -Path $projDataDir -Destination $buildProjDataDir -Recurse -Force

    $launcherPath = Join-Path $buildDir "run-rastertoolbox.cmd"
    @"
@echo off
setlocal
set "APP_DIR=%~dp0"
set "GDAL_DATA=%APP_DIR%share\gdal"
set "PROJ_DATA=%APP_DIR%share\proj"
set "PROJ_LIB=%APP_DIR%share\proj"
set "PATH=%APP_DIR%;%PATH%"
"%APP_DIR%rastertoolbox.exe" %*
"@ | Set-Content -Path $launcherPath -Encoding ASCII
}

$commands = switch ($Action) {
    "configure" { @("`"$cmakeExe`" --preset $preset") }
    "build" { @(Get-ConfigureCommands) + @("`"$cmakeExe`" --build --preset $preset") }
    "deploy" { @() }
    "test" { @("`"$ctestExe`" --preset $preset") }
    "smoke" { @("$buildDir\\rastertoolbox.exe --smoke-startup") }
    "run" { @("$buildDir\\rastertoolbox.exe") }
    "all" {
        @(Get-ConfigureCommands) + @(
            "`"$cmakeExe`" --build --preset $preset",
            "`"$ctestExe`" --preset $preset",
            "$buildDir\\rastertoolbox.exe --smoke-startup"
        )
    }
}

$commandText = @(
    "set `"PATH=$baseSystemPath`"",
    "call `"$vcvars`"",
    "set VCPKG_ROOT=$vcpkgRoot",
    "set VCPKG_FORCE_SYSTEM_BINARIES=1",
    "set `"LIB=$sdkLibUmDir;$sdkLibUcrtDir;!LIB!`"",
    "set `"INCLUDE=$sdkIncludeUcrtDir;$sdkIncludeUmDir;$sdkIncludeSharedDir;$sdkIncludeWinRTDir;$sdkIncludeCppWinRTDir;!INCLUDE!`"",
    "set `"PATH=$vcpkgBinDir;$sevenZipDir;$sevenZrDir;$sdkBinDir;$ninjaDir;$qtBinDir;!PATH!`"",
    "cd /d `"$repoRoot`""
)

if (Test-Path $gdalDataDir) {
    $commandText += "set GDAL_DATA=$gdalDataDir"
}

if (Test-Path $projDataDir) {
    $commandText += "set PROJ_DATA=$projDataDir"
    $commandText += "set PROJ_LIB=$projDataDir"
}

$commandText += $commands

if ($commands.Count -gt 0) {
    & "$env:SystemRoot\System32\cmd.exe" /V:ON /c ($commandText -join " && ")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($Action -in @("build", "deploy", "all")) {
    Invoke-LocalDeploy
}

exit 0
