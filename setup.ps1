# Downloads the pinned toolchain into tools\ (about 350 MB, Windows only).
# Usage: powershell -ExecutionPolicy Bypass -File setup.ps1

$ErrorActionPreference = 'Stop'

$Tools = Join-Path $PSScriptRoot 'tools'

$LlvmUrl = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip'
$OpenOrbisUrl = 'https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.3/toolchain-llvm-18.2.zip'
$SdkUrl = 'https://github.com/GoldHEN/GoldHEN_Plugins_SDK/archive/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7.zip'

# Remove-Item cannot delete paths longer than 260 characters; rd with the
# \\?\ prefix can.
function Remove-Tree([string]$Dir) {
    # Only ever delete inside this project's tools folder.
    if (-not $Dir.StartsWith("$Tools\", [StringComparison]::OrdinalIgnoreCase)) { throw "refusing to delete $Dir" }
    if (Test-Path $Dir) {
        cmd /c "rd /s /q `"\\?\$Dir`""
        if (Test-Path $Dir) { throw "could not delete $Dir" }
    }
}

# Windows' tar (bsdtar) instead of Expand-Archive: it copes with paths longer
# than 260 characters and is much faster.
function Get-Archive([string]$Url, [string]$Dir, [string[]]$Exclude = @()) {
    Remove-Tree $Dir # leftovers of an interrupted run
    New-Item -ItemType Directory -Force $Dir | Out-Null
    $file = Join-Path $Dir 'download.zip'
    Write-Host "[+] Downloading $Url"
    curl.exe -sSL --fail -o $file $Url
    if ($LASTEXITCODE -ne 0) { throw "download failed: $Url" }
    $tarArgs = @('-xf', $file, '-C', $Dir)
    foreach ($pattern in $Exclude) { $tarArgs += @('--exclude', $pattern) }
    tar @tarArgs
    if ($LASTEXITCODE -ne 0) { throw "failed to extract $Url" }
    Remove-Item $file
}

function Test-Tool([string]$Dir, [string]$Filter) {
    (Test-Path $Dir) -and $null -ne (Get-ChildItem $Dir -Recurse -Filter $Filter -ErrorAction SilentlyContinue | Select-Object -First 1)
}

# Compiler. Python and the ARM/x86 Windows targets are not needed.
$llvm = Join-Path $Tools 'llvm'
if (-not (Test-Tool $llvm 'ld.lld.exe')) {
    Get-Archive $LlvmUrl $llvm @('*/python/*', '*/aarch64-w64-mingw32/*', '*/armv7-w64-mingw32/*', '*/i686-w64-mingw32/*')
}

# OpenOrbis: a zip that contains a tarball.
$openOrbis = Join-Path $Tools 'openorbis'
if (-not (Test-Tool $openOrbis 'create-fself.exe')) {
    Get-Archive $OpenOrbisUrl $openOrbis
    $tarball = Get-ChildItem $openOrbis -Filter '*.tar.gz' | Select-Object -First 1
    tar -xzf $tarball.FullName -C $openOrbis
    if ($LASTEXITCODE -ne 0) { throw 'failed to extract the OpenOrbis toolchain' }
    Remove-Item $tarball.FullName
}

$sdk = Join-Path $Tools 'goldhen-sdk-src'
if (-not (Test-Tool $sdk 'Detour.h')) {
    Get-Archive $SdkUrl $sdk
}

Write-Host '[+] Toolchain ready. Now run build.ps1'
