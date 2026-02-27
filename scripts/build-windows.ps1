param(
  [string]$Configuration = "Release",
  [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

Write-Host "== Build script starting =="
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"

$repoRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $repoRoot

# Find first .sln
$sln = Get-ChildItem -Path $repoRoot -Filter *.sln -Recurse | Select-Object -First 1
if (-not $sln) {
  Write-Host "No .sln found yet. Nothing to build."
  exit 0
}

Write-Host "Building solution: $($sln.FullName)"

# Use VS DevCmd via vswhere (available on GitHub runner)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsPath) { throw "Visual Studio not found." }

$devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $devCmd)) { throw "VsDevCmd not found: $devCmd" }

cmd /c "`"$devCmd`" -arch=amd64 && msbuild `"$($sln.FullName)`" /m /p:Configuration=$Configuration /p:Platform=$Platform"

Write-Host "== Build script done =="
