param([string]$Action = 'configure', [string]$Configuration = 'Debug', [string[]]$Targets)
$treeRoot = 'C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f'
if ((Get-Location).Path -ne (Resolve-Path -LiteralPath $treeRoot).Path) {
    throw 'Run this evidence driver only from its bound Tree worktree.'
}
if (!$Targets.Count) {
    $Targets = @('rynui_tree_component_tests','rynui_tree_model_tests','rynui_tree_theme_tests',
        'rynui_portable_focus_order','rynui_portable_focus_state','rynui_portable_focus_lifecycle',
        'rynui_portable_interaction_registry','rynui_portable_layout_style','rynui_portable_theme_runtime',
        'rynui_portable_theme_algorithm','rynui_steps_component_tests','rynui_portable_menu_component',
        'rynui_collapse_component_tests','rynui_portable_select_component','rynui_portable_input_component',
        'rynui_portable_button_component','rynui_avatar_component_tests','rynui_avatar_theme_tests',
        'rynui_timeline_integration_tests')
}
$ErrorActionPreference = 'Stop'
$env:Path = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0;C:/Program Files (x86)/Microsoft Visual Studio/Installer"
Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -Arch amd64 -HostArch amd64
$ErrorActionPreference = 'Continue'
New-Item -ItemType Directory -Force -Path out/tree-logs | Out-Null
if ($Action -eq 'configure') {
    New-Item -ItemType Directory -Force -Path out/build/windows-msvc-headless/_deps/rynui-validation-fonts | Out-Null
    Copy-Item -LiteralPath 'D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSans-Regular.ttf','D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSansCJKsc-Regular.otf','D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSansARABIC-Regular.ttf','D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSansHEBREW-Regular.ttf' -Destination out/build/windows-msvc-headless/_deps/rynui-validation-fonts
    $configureArgs = @('--preset','windows-msvc-headless')
    foreach ($dependency in @('RYNUI_FREETYPE','RYNUI_HARFBUZZ','RYNUI_SHEENBIDI','RYNUI_UTF8PROC')) {
        $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_${dependency}=D:/code/RynUI/out/build/windows-msvc-headless/_deps/$($dependency.ToLowerInvariant())-src"
    }
    & cmake @configureArgs 2>&1 | Tee-Object out/tree-logs/configure.log
} elseif ($Action -eq 'build') {
    $buildArgs = @('--build','--preset',"windows-msvc-headless-$($Configuration.ToLowerInvariant())",'--parallel','2')
    if ($Targets.Count) { $buildArgs += '--target'; $buildArgs += $Targets }
    & cmake @buildArgs 2>&1 | Tee-Object "out/tree-logs/build-$Configuration.log"
} elseif ($Action -eq 'test') {
    & ctest --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" -R '^rynui\.(tree_.*|avatar_.*|steps_component|collapse_component|timeline_integration|portable\.(focus_order|focus_state|focus_lifecycle|interaction_registry|layout_style|theme_runtime|theme_algorithm|menu_component|select_component|input_component|button_component))$' --output-on-failure --timeout 45 2>&1 | Tee-Object "out/tree-logs/test-$Configuration.log"
}
exit $LASTEXITCODE
