[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$buildDir = Join-Path $repoRoot 'build/ci-windows'
$exePath = Join-Path $buildDir 'rastertoolbox.exe'
$triplet = 'x64-windows'
$vcpkgInstalled = Join-Path $repoRoot "vcpkg_installed/$triplet"
$vcpkgBinDir = Join-Path $vcpkgInstalled 'bin'
$gdalDataDir = Join-Path $vcpkgInstalled 'share/gdal'
$projDataDir = Join-Path $vcpkgInstalled 'share/proj'

$distRoot = Join-Path $repoRoot 'dist/windows-release'
$distShare = Join-Path $distRoot 'share'
$zipPath = Join-Path $repoRoot "dist/RasterToolbox-$Tag-windows-x64.zip"

if (-not (Test-Path $exePath -PathType Leaf)) { throw "missing executable: $exePath" }
if (-not (Test-Path $vcpkgBinDir -PathType Container)) { throw "missing vcpkg bin directory: $vcpkgBinDir" }
if (-not (Test-Path $gdalDataDir -PathType Container)) { throw "missing GDAL data directory: $gdalDataDir" }
if (-not (Test-Path $projDataDir -PathType Container)) { throw "missing PROJ data directory: $projDataDir" }

$windeployqt = Get-ChildItem -Path (Join-Path $vcpkgInstalled 'tools') -Recurse -File -Filter 'windeployqt.exe' |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $windeployqt) { throw 'windeployqt.exe not found in vcpkg installed tools' }

if (Test-Path $distRoot) { Remove-Item -Path $distRoot -Recurse -Force }
if (Test-Path $zipPath) { Remove-Item -Path $zipPath -Force }
New-Item -Path $distShare -ItemType Directory -Force | Out-Null

$distExePath = Join-Path $distRoot 'rastertoolbox.exe'
Copy-Item -Path $exePath -Destination $distExePath -Force

& $windeployqt --release --compiler-runtime --dir $distRoot $distExePath
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

Get-ChildItem -Path $vcpkgBinDir -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination (Join-Path $distRoot $_.Name) -Force
}

Copy-Item -Path $gdalDataDir -Destination (Join-Path $distShare 'gdal') -Recurse -Force
Copy-Item -Path $projDataDir -Destination (Join-Path $distShare 'proj') -Recurse -Force

Compress-Archive -Path (Join-Path $distRoot '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force
if (-not (Test-Path $zipPath -PathType Leaf)) { throw "failed to create release zip: $zipPath" }

Write-Host "packaged $zipPath"
