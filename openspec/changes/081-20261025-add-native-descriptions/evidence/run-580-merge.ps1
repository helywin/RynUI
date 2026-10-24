param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$TestFilter = '^rynui\.(descriptions_(model|public_api|component|theme|integration_(popup|cleanup|boundary|scene)|region_clip_(horizontal|bordered|vertical|vertical_bordered|rich)|shared_theme)|time_picker_(theme|integration)|time_navigation_merge|portable\.(theme_(algorithm|runtime)|backend_boundaries))$'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
Push-Location -LiteralPath $root
try {
    $taskGitDirectory = Split-Path (Get-Command git).Source
    Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
    $env:Path = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0"
    Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -Arch amd64 -HostArch amd64
    $env:Path += ";$taskGitDirectory"
    $ErrorActionPreference = 'Continue'
    $suffix = $Configuration.ToLowerInvariant()
    $targets = @(
        'rynui_descriptions_model_tests', 'rynui_descriptions_public_api_tests',
        'rynui_descriptions_component_tests', 'rynui_descriptions_theme_tests',
        'rynui_descriptions_integration_tests', 'rynui_descriptions_region_clip_tests',
        'rynui_descriptions_shared_theme_tests', 'rynui_time_picker_theme_tests',
        'rynui_time_picker_integration_tests', 'rynui_time_navigation_merge_tests',
        'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime'
    )
    cmake --preset windows-msvc-headless
    if ($LASTEXITCODE) { throw 'configure failed' }
    cmake --build --preset "windows-msvc-headless-$suffix" --parallel 4 --target @targets
    if ($LASTEXITCODE) { throw 'build failed' }
    & "./out/build/windows-msvc-headless/tests/portable/$Configuration/rynui_portable_theme_algorithm.exe" --write-goldens
    if ($LASTEXITCODE) { throw 'fresh golden generation failed' }
    & 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' "$PSScriptRoot/verify_preservation.py"
    if ($LASTEXITCODE) { throw 'published main and local branch preservation failed' }
    if ($Configuration -eq 'Debug') {
        & 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' "$PSScriptRoot/extract-doc-examples.py"
        if ($LASTEXITCODE) { throw 'documentation extraction failed' }
        cl /nologo /std:c++20 /EHsc /utf-8 /c /Iinclude out/descriptions-doc-examples.cpp /Foout/descriptions-doc-examples.obj
        if ($LASTEXITCODE) { throw 'documentation compilation failed' }
    }
    ctest --preset "windows-msvc-headless-$suffix" -R $TestFilter --output-on-failure
    if ($LASTEXITCODE) { throw 'CTest failed' }
} finally {
    Pop-Location
}
