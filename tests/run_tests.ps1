# Compiles the PS4-independent sources for Windows and runs their unit tests.
# Usage: powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 [-ToolsDir <path>]

param(
    [string]$ToolsDir = ''
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
if (-not $ToolsDir) {
    $ToolsDir = Join-Path $Root 'tools'
    if (-not (Test-Path $ToolsDir)) { $ToolsDir = Join-Path $Root '..\tools' }
}
$Clang = Get-ChildItem (Join-Path $ToolsDir 'llvm') -Directory -Filter 'llvm-mingw-*' -ErrorAction SilentlyContinue |
         Select-Object -First 1 | ForEach-Object { Join-Path $_.FullName 'bin\clang.exe' }
if (-not $Clang) { throw "Compiler not found in $ToolsDir. Run setup.ps1 first." }
$Exe = Join-Path $Root 'build\tests.exe'

New-Item -ItemType Directory -Force (Join-Path $Root 'build') | Out-Null
$Sources = @('xinput.c', 'translate.c', 'gyro.c', 'config.c') | ForEach-Object { Join-Path $Root "source\$_" }
& $Clang -Wall -Werror -I (Join-Path $Root 'include') (Join-Path $PSScriptRoot 'tests.c') @Sources -o $Exe
if ($LASTEXITCODE -ne 0) { throw 'test build failed' }
& $Exe
exit $LASTEXITCODE
