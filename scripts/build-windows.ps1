[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [ValidateSet('BUNDLED', 'SYSTEM')]
    [string] $DependencyMode = 'BUNDLED',

    [string] $Sdl3Root,

    [string] $FreeTypeRoot,

    [string] $HarfBuzzRoot,

    [string] $ShadercrossExecutable,

    [string] $LatinFontFile,

    [string] $CjkFontFile,

    [switch] $Fresh,

    [switch] $SkipTests
)

$ErrorActionPreference = 'Stop'

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not (Test-Path -LiteralPath $vswherePath)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$installationJson = & $vswherePath -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json
if($LASTEXITCODE -ne 0) {
    throw "vswhere.exe failed with exit code $LASTEXITCODE."
}

$installations = @($installationJson | ConvertFrom-Json)
if($installations.Count -eq 0) {
    throw 'A Visual Studio installation with the MSVC x64 tools was not found.'
}

$vsInstallPath = $installations[0].installationPath
$devShellModule = Join-Path $vsInstallPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if(-not (Test-Path -LiteralPath $devShellModule)) {
    throw "Visual Studio Developer Shell module was not found: $devShellModule"
}

Import-Module $devShellModule

# Enter-VsDevShell delegates to cmd.exe, whose command-line limit can be hit by
# unusually long inherited PATH values. Visual Studio adds its own CMake and
# Ninja locations, so start from the stable Windows paths before importing the
# x64 toolchain environment.
$env:Path = @(
    (Join-Path $env:SystemRoot 'System32'),
    $env:SystemRoot,
    (Join-Path $env:SystemRoot 'System32\Wbem'),
    (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0')
) -join ';'

Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation `
    -Arch amd64 -HostArch amd64

$presetSuffix = $Configuration.ToLowerInvariant()
$configurePreset = if($DependencyMode -eq 'SYSTEM') {
    'windows-msvc-system'
} else {
    'windows-msvc'
}

$configureArguments = @('--preset', $configurePreset)
if($Fresh) {
    $configureArguments = @('--fresh') + $configureArguments
}
if($Sdl3Root) {
    if($DependencyMode -ne 'SYSTEM') {
        throw '-Sdl3Root can only be used with -DependencyMode SYSTEM.'
    }

    $resolvedSdl3Root = (Resolve-Path -LiteralPath $Sdl3Root).Path
    $configureArguments += "-DSDL3_ROOT=$resolvedSdl3Root"
}
foreach($systemRoot in @(
    @{ Name = 'FreeTypeRoot'; Value = $FreeTypeRoot; Cache = 'Freetype_ROOT' },
    @{ Name = 'HarfBuzzRoot'; Value = $HarfBuzzRoot; Cache = 'harfbuzz_ROOT' }
)) {
    if($systemRoot.Value) {
        if($DependencyMode -ne 'SYSTEM') {
            throw "-$($systemRoot.Name) can only be used with -DependencyMode SYSTEM."
        }
        $resolvedRoot = (Resolve-Path -LiteralPath $systemRoot.Value).Path
        $configureArguments += "-D$($systemRoot.Cache)=$resolvedRoot"
    }
}
if($ShadercrossExecutable) {
    $resolvedShadercrossExecutable = (Resolve-Path -LiteralPath $ShadercrossExecutable).Path
    $configureArguments += "-DRYNUI_SHADERCROSS_EXECUTABLE=$resolvedShadercrossExecutable"
}
foreach($font in @(
    @{ Name = 'LatinFontFile'; Value = $LatinFontFile; Cache = 'RYNUI_SYSTEM_LATIN_FONT_FILE' },
    @{ Name = 'CjkFontFile'; Value = $CjkFontFile; Cache = 'RYNUI_SYSTEM_CJK_FONT_FILE' }
)) {
    if($font.Value) {
        if($DependencyMode -ne 'SYSTEM') {
            throw "-$($font.Name) can only be used with -DependencyMode SYSTEM."
        }
        $resolvedFont = (Resolve-Path -LiteralPath $font.Value).Path
        $configureArguments += "-D$($font.Cache)=$resolvedFont"
    }
}

cmake @configureArguments
if($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

cmake --build --preset "$configurePreset-$presetSuffix"
if($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}

if(-not $SkipTests) {
    ctest --preset "$configurePreset-$presetSuffix"
    if($LASTEXITCODE -ne 0) {
        throw "CTest failed with exit code $LASTEXITCODE."
    }
}
