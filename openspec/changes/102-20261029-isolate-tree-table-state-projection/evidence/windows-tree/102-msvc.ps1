param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$Stage='build', [string]$Label='before', [int]$Count=128, [int]$Iterations=64, [int]$Round=1, [string]$Component='tree')
$ErrorActionPreference='Stop'
Set-Location -LiteralPath 'D:/code/RynUI'
$taskInstaller=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer'
$env:PATH="$taskInstaller;$env:PATH"
$taskVs=& (Join-Path $taskInstaller 'vswhere.exe') -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module (Join-Path $taskVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $taskVs -SkipAutomaticLocation -Arch amd64 -HostArch amd64 | Out-Null
$env:VSLANG='1033'
$env:PYTHONDONTWRITEBYTECODE='1'
$taskPreset='windows-msvc-headless-'+$Configuration.ToLowerInvariant()
$taskPrefix="out/102-$Label-$Stage-$Configuration"
if($Stage -eq 'bench') { $taskPrefix += "-$Component-$Count-r$Round" }
$taskTimer=[Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference='Continue'
switch($Stage) {
 'build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_state_projection_tests rynui_state_projection_benchmark *> ($taskPrefix+'.log') }
 'baseline-test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.state_projection_(tree_local|table_cache)$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'probe-build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_glyph_replace_tests rynui_portable_text_scene_service *> ($taskPrefix+'.log') }
 'probe-test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.(glyph_replace|portable\.text_scene_service)$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'tree-build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_state_projection_tests rynui_state_projection_benchmark rynui_tree_component_tests rynui_tree_model_tests rynui_tree_theme_tests rynui_tree_review_tests rynui_tree_r2_tests rynui_tree_select_component_tests rynui_tree_select_integration_tests rynui_tree_select_model_tests rynui_tree_select_theme_tests rynui_tree_select_review_tests rynui_tree_select_r2_tests rynui_tree_select_r3_tests rynui_portable_focus_order rynui_portable_focus_lifecycle rynui_portable_input_component *> ($taskPrefix+'.log') }
 'tree-test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.(state_projection_tree_.*|tree_.*|tree_select_(component|integration).*|portable\.(focus_order|focus_lifecycle|input_component))$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'bench' { $taskArgs=@($Component,$Count,$Iterations); if($Label -eq 'before') {$taskArgs+='--baseline'}; & ('.\out\build\windows-msvc-headless\tests\'+$Configuration+'\rynui_state_projection_benchmark.exe') @taskArgs *> ($taskPrefix+'.log') }
 default { throw "Unknown stage: $Stage" }
}
$taskExit=$LASTEXITCODE
$taskTimer.Stop()
[ordered]@{stage=$Stage;label=$Label;configuration=$Configuration;component=$Component;count=$Count;iterations=$Iterations;round=$Round;exit=$taskExit;seconds=$taskTimer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($taskPrefix+'.json')
Write-Output "Stage=$Stage Label=$Label Configuration=$Configuration Component=$Component Count=$Count Round=$Round exit=$taskExit seconds=$($taskTimer.Elapsed.TotalSeconds)"
exit $taskExit
