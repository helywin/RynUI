param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$TestFilter = '^rynui\.(descriptions_region_clip_(horizontal|bordered|vertical|vertical_bordered|rich|selection|selection_modal)|portable\.(selection_component|switch_features|radio_features|modal_component))$'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
Push-Location -LiteralPath $root
try {
    Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
    $env:Path = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0"
    Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -Arch amd64 -HostArch amd64
    $ErrorActionPreference = 'Continue'
    $suffix = $Configuration.ToLowerInvariant()
    $targets = @('rynui_descriptions_region_clip_tests', 'rynui_portable_selection_component',
                 'rynui_portable_switch_features', 'rynui_portable_radio_features', 'rynui_portable_modal_component')
    cmake --preset windows-msvc-headless
    if ($LASTEXITCODE) { throw 'configure failed' }
    cmake --build --preset "windows-msvc-headless-$suffix" --parallel 4 --target @targets
    if ($LASTEXITCODE) { throw 'build failed' }
    ctest --preset "windows-msvc-headless-$suffix" -R $TestFilter --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
} finally {
    Pop-Location
}
