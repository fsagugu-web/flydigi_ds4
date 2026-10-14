# Builds out\flydigi_ds4.prx on Windows with llvm-mingw clang + OpenOrbis.
# Run setup.ps1 once first (it downloads the toolchain into tools\).
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1 [-ToolsDir <path>]

param(
    [string]$ToolsDir = ''
)

$ErrorActionPreference = 'Stop'

$Root = $PSScriptRoot
if (-not $ToolsDir) {
    $ToolsDir = Join-Path $Root 'tools'
    if (-not (Test-Path $ToolsDir)) { $ToolsDir = Join-Path $Root '..\tools' }
}
$Toolchain = Join-Path $ToolsDir 'openorbis\OpenOrbis\PS4Toolchain'
$LlvmBin   = Get-ChildItem (Join-Path $ToolsDir 'llvm') -Directory -Filter 'llvm-mingw-*' -ErrorAction SilentlyContinue |
             Select-Object -First 1 | ForEach-Object { Join-Path $_.FullName 'bin' }
$SdkSrc    = Get-ChildItem (Join-Path $ToolsDir 'goldhen-sdk-src') -Directory -ErrorAction SilentlyContinue |
             Select-Object -First 1 -ExpandProperty FullName
if (-not $LlvmBin -or -not $SdkSrc -or -not (Test-Path $Toolchain)) {
    throw "Toolchain not found in $ToolsDir. Run setup.ps1 first."
}
$SdkBuild  = Join-Path $ToolsDir 'goldhen-sdk-build'
$BuildDir  = Join-Path $Root 'build'
$OutDir    = Join-Path $Root 'out'
$Name      = 'flydigi_ds4'

$Clang = Join-Path $LlvmBin 'clang.exe'
$Lld   = Join-Path $LlvmBin 'ld.lld.exe'
$Ar    = Join-Path $LlvmBin 'llvm-ar.exe'
$Fself = Join-Path $Toolchain 'bin\windows\create-fself.exe'

# create-fself refuses to run without it
$env:OO_PS4_TOOLCHAIN = $Toolchain

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($Exe)) failed with exit code $LASTEXITCODE" }
}

$Defines = @('-D_BSD_SOURCE=1', '-D__BSD_VISIBLE=1', '-D__PS4__=1', '-DPS4=1', '-DORBIS=1', '-D__ORBIS__=1',
             '-DOO=1', '-D__OPENORBIS__=1', '-D__OOPS4__=1', '-DOOPS4=1', '-D__FINAL__=1')
$PsCFlags = @('--no-default-config', '--target=x86_64-pc-freebsd12-elf', '-march=btver2', '-mtune=btver2',
              '-fPIC', '-funwind-tables', '-O2', '-Wall', '-isysroot', $Toolchain,
              '-isystem', (Join-Path $Toolchain 'include')) + $Defines

# 1. GoldHEN SDK static library (hooks, syscalls, prx CRT)
$SdkLib = Join-Path $SdkBuild 'libGoldHEN_Hook.a'
$SdkCrt = Join-Path $SdkBuild 'crtprx.o'
if (-not (Test-Path $SdkLib)) {
    Write-Host '[+] Building GoldHEN SDK'
    New-Item -ItemType Directory -Force $SdkBuild | Out-Null
    $sdkObjs = @()
    foreach ($src in Get-ChildItem (Join-Path $SdkSrc 'source\*.c')) {
        $obj = Join-Path $SdkBuild ($src.BaseName + '.o')
        Invoke-Tool $Clang ($PsCFlags + @('-std=c11', '-I', (Join-Path $SdkSrc 'include'), '-c', $src.FullName, '-o', $obj))
        $sdkObjs += $obj
    }
    Invoke-Tool $Clang @('--no-default-config', '-target', 'x86_64-pc-linux-gnu', '-ffreestanding', '-nostdlib',
                         '-fno-builtin', '-fPIC', '-isysroot', $Toolchain, '-isystem', (Join-Path $Toolchain 'include'),
                         '-c', (Join-Path $SdkSrc 'crt\crtprx.c'), '-o', $SdkCrt)
    Invoke-Tool $Ar (@('--format=bsd', 'rcs', $SdkLib) + $sdkObjs + @($SdkCrt))
}

# 2. Plugin objects
Write-Host "[+] Building $Name"
New-Item -ItemType Directory -Force $BuildDir, $OutDir | Out-Null
$objs = @()
foreach ($src in Get-ChildItem (Join-Path $Root 'source\*.c')) {
    $obj = Join-Path $BuildDir ($src.BaseName + '.o')
    Invoke-Tool $Clang ($PsCFlags + @('-I', (Join-Path $SdkSrc 'include'), '-I', (Join-Path $Root 'include'),
                                      '-c', $src.FullName, '-o', $obj))
    $objs += $obj
}

# 3. Link and sign as a PRX
$Elf  = Join-Path $BuildDir "$Name.elf"
$Oelf = Join-Path $BuildDir "$Name.oelf"
$Prx  = Join-Path $OutDir "$Name.prx"
$Libs = @('-lGoldHEN_Hook', '-lkernel', '-lSceLibcInternal', '-lSceSysmodule', '-lScePad', '-lSceSystemService')
Invoke-Tool $Lld (@('-m', 'elf_x86_64', '-pie', '-e', '_init', '--script', (Join-Path $Toolchain 'link.x'),
                    '--eh-frame-hdr', $SdkCrt) + $objs + @('-o', $Elf, '-L', (Join-Path $Toolchain 'lib'),
                    '-L', $SdkBuild) + $Libs)
# create-fself embeds the input path in the PRX: relative paths keep the
# builder's folder (and Windows user name) out of it and make builds identical.
Push-Location $BuildDir
try {
    Invoke-Tool $Fself @("-in=$Name.elf", "-out=$Name.oelf", "--lib=$Name.prx", '--paid', '0x3800000000000011')
} finally {
    Pop-Location
}
Move-Item (Join-Path $BuildDir "$Name.prx") $Prx -Force
Copy-Item (Join-Path $Root 'config\flydigi_ds4.ini') $OutDir -Force

Write-Host "[+] Done: $Prx"
