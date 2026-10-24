param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
Push-Location -LiteralPath $root
try {
    $taskGitDirectory = Split-Path (Get-Command git -ErrorAction Stop).Source
    Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
    $env:Path = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0"
    Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -Arch amd64 -HostArch amd64
    $env:Path += ";$taskGitDirectory"
    $suffix = $Configuration.ToLowerInvariant()
    $targets = @(
        'rynui_descriptions_model_tests', 'rynui_descriptions_public_api_tests',
        'rynui_descriptions_component_tests', 'rynui_descriptions_theme_tests', 'rynui_descriptions_integration_tests',
        'rynui_breadcrumb_component_tests', 'rynui_breadcrumb_theme_tests', 'rynui_steps_component_tests', 'rynui_steps_theme_tests',
        'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime',
        'rynui_date_picker_component_tests', 'rynui_date_picker_theme_tests', 'rynui_date_picker_review_tests',
        'rynui_form_component_tests', 'rynui_form_theme_tests', 'rynui_feedback_component_tests', 'rynui_feedback_theme_tests',
        'rynui_collapse_component_tests', 'rynui_collapse_theme_tests',
        'rynui_portable_button_component', 'rynui_portable_modal_component', 'rynui_portable_modal_boundary',
        'rynui_portable_prop', 'rynui_portable_select_component', 'rynui_portable_select_theme',
        'rynui_portable_input_component', 'rynui_portable_focus_lifecycle'
    )
    # Windows PowerShell 5 wraps redirected native stderr as nonterminating errors.
    $ErrorActionPreference = 'Continue'
    cmake --preset windows-msvc-headless
    if ($LASTEXITCODE) { throw 'configure failed' }
    cmake --build --preset "windows-msvc-headless-$suffix" --parallel 4 --target @targets
    if ($LASTEXITCODE) { throw 'build failed' }
    & "./out/build/windows-msvc-headless/tests/portable/$Configuration/rynui_portable_theme_algorithm.exe" --write-goldens
    if ($LASTEXITCODE) { throw 'fresh golden generation failed' }
    & 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' "$PSScriptRoot/verify_preservation.py"
    if ($LASTEXITCODE) { throw 'dual baseline preservation failed' }
    if ($Configuration -eq 'Debug') {
        & 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' "$PSScriptRoot/extract-doc-examples.py"
        if ($LASTEXITCODE) { throw 'documentation extraction failed' }
        cl /nologo /std:c++20 /EHsc /utf-8 /c /Iinclude out/descriptions-doc-examples.cpp /Foout/descriptions-doc-examples.obj
        if ($LASTEXITCODE) { throw 'documentation examples failed to compile' }
    }
    $filter = '^rynui\.(descriptions_(model|public_api|component|theme|integration_(popup|cleanup|boundary|scene))|(breadcrumb|steps)_(component|theme)|date_picker_(component|theme|review_(preflight|form_reset))|form_(component|theme)|feedback_(component|theme)|collapse_(component|theme)|portable\.(theme_(algorithm|runtime)|button_component|modal_(component|boundary)|prop|select_(component|theme)|input_component|focus_lifecycle|backend_boundaries))$'
    ctest --preset "windows-msvc-headless-$suffix" -R $filter
    if ($LASTEXITCODE) { throw 'CTest failed' }
} finally {
    Pop-Location
}
