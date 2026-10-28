param([ValidateSet('build','test')][string]$Action = 'build',
      [ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$treeRoot = 'C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f'
if ((Get-Location).Path -ne (Resolve-Path -LiteralPath $treeRoot).Path) {
    throw 'Run only in the bound Tree worktree.'
}
$env:Path = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/WindowsPowerShell/v1.0;C:/Program Files (x86)/Microsoft Visual Studio/Installer"
Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -Arch amd64 -HostArch amd64
$ErrorActionPreference = 'Continue'
$treeLogs = 'openspec/changes/094-20261026-add-native-tree/evidence/logs'
if ($Action -eq 'build') {
    & cmake --build --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" --parallel 2 --target rynui_tree_review_tests rynui_tree_component_tests rynui_tree_model_tests rynui_tree_theme_tests rynui_portable_modal_component rynui_portable_select_component rynui_portable_input_component rynui_portable_focus_lifecycle rynui_portable_text_component rynui_portable_text_component_frame 2>&1 |
        ForEach-Object { $_.ToString() } | Tee-Object "$treeLogs/r1-build-$Configuration.log"
} else {
    & ctest --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" -R '^rynui\.(tree_.*|portable\.(modal_component|select_component|input_component|focus_lifecycle|text_component(_frame)?))$' -V --timeout 45 2>&1 |
        ForEach-Object { $_.ToString() } | Tee-Object "$treeLogs/r1-after-$Configuration.log"
}
exit $LASTEXITCODE
