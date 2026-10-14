# Builds and assembles what gets published:
#   ..\publicar\repositorio\           clean copy of the repository to upload to GitHub
#   ..\publicar\flydigi_ds4-v<ver>.zip  release archive for players
# Usage: powershell -ExecutionPolicy Bypass -File package.ps1 [-Version 1.3]

param(
    [string]$Version = '1.3'
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Publish = Join-Path (Split-Path $Root -Parent) 'publicar'
$Repo = Join-Path $Publish 'repositorio'
$Stage = Join-Path $Publish 'zip'
$Zip = Join-Path $Publish "flydigi_ds4-v$Version.zip"

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'tests\run_tests.ps1')
if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

foreach ($dir in $Repo, $Stage) {
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    New-Item -ItemType Directory -Force $dir | Out-Null
}
if (Test-Path $Zip) { Remove-Item -Force $Zip }

# Repository: everything except build output, local tools and dev leftovers.
$exclude = @('build', 'out', 'tools', 'libs.txt')
Get-ChildItem $Root -Force | Where-Object { $_.Name -notin $exclude } | Copy-Item -Destination $Repo -Recurse -Force

# Release archive for players.
Copy-Item (Join-Path $Root 'out\flydigi_ds4.prx') $Stage
Copy-Item (Join-Path $Root 'config\flydigi_ds4.ini') $Stage
Copy-Item (Join-Path $Root 'release\LEIA-ME.txt') $Stage
Copy-Item (Join-Path $Root 'release\plugins.ini') $Stage
Copy-Item (Join-Path $Root 'LICENSE') $Stage
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $Zip
Remove-Item -Recurse -Force $Stage

$hash = (Get-FileHash (Join-Path $Root 'out\flydigi_ds4.prx') -Algorithm SHA256).Hash
Write-Host "[+] Repository: $Repo"
Write-Host "[+] Release:    $Zip"
Write-Host "[+] SHA-256 flydigi_ds4.prx: $hash"
