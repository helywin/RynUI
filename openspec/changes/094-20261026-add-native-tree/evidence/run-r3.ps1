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
$treeTargets = @(
    'rynui_tree_component_tests', 'rynui_tree_model_tests', 'rynui_tree_theme_tests',
    'rynui_tree_review_tests', 'rynui_tree_r2_tests',
    'rynui_auto_complete_component_tests', 'rynui_auto_complete_model_tests', 'rynui_auto_complete_theme_tests',
    'rynui_auto_complete_integration_tests', 'rynui_auto_complete_review_tests',
    'rynui_auto_complete_r2_tests', 'rynui_auto_complete_r3_tests', 'rynui_component_failure_retirement_tests',
    'rynui_portable_focus_order', 'rynui_portable_focus_state', 'rynui_portable_focus_lifecycle',
    'rynui_portable_pointer_route', 'rynui_portable_input_component', 'rynui_portable_input_actions',
    'rynui_portable_select_component', 'rynui_portable_layout_style', 'rynui_portable_modal_component',
    'rynui_popover_integration_tests', 'rynui_popover_review_tests', 'rynui_time_picker_review_tests',
    'rynui_date_picker_review_tests', 'rynui_form_component_tests',
    'rynui_avatar_component_tests', 'rynui_avatar_theme_tests'
)
$treeRegex = '^rynui\.(tree_.*|auto_complete_.*|component_failure_retirement|portable\.(focus_order|focus_state|focus_lifecycle|pointer_route|input_component|input_actions|select_component|layout_style|modal_component)|popover_children|popover_review_(focus|escape)|time_picker_review_(focus|row_focus)|date_picker_review_form_reset|form_component|avatar_(lifecycle|theme))$'
$treeLogs = 'openspec/changes/094-20261026-add-native-tree/evidence/logs'
$ErrorActionPreference = 'Continue'
if ($Action -eq 'build') {
    & cmake --build --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" --parallel 2 --target @treeTargets 2>&1 |
        ForEach-Object { $_.ToString() } | Tee-Object "$treeLogs/r3-build-$Configuration.log"
    exit $LASTEXITCODE
}
$treeList = & ctest --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" -R $treeRegex --show-only=json-v1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$treeInventory = $treeList | ConvertFrom-Json
if ($treeInventory.tests.Count -ne 62 -or ($treeInventory.tests.name | Sort-Object -Unique).Count -ne 62) {
    throw 'Expected exactly 62 distinct targeted CTests.'
}
[System.IO.File]::WriteAllText((Join-Path (Get-Location) "$treeLogs/r3-tests-$Configuration.json"), ($treeList -join "`n"), [System.Text.UTF8Encoding]::new($false))
& ctest --preset "windows-msvc-headless-$($Configuration.ToLowerInvariant())" -R $treeRegex -V --timeout 45 2>&1 |
    ForEach-Object { $_.ToString() } | Tee-Object "$treeLogs/r3-after-$Configuration.log"
exit $LASTEXITCODE
