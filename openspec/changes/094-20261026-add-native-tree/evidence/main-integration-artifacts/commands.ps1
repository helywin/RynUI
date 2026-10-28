param([ValidateSet('configure','build','export','test','probe')][string]$Stage,
      [ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
Set-Location -LiteralPath 'D:/code/RynUI'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsInstall=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module (Join-Path $vsInstall 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
$env:PATH="$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
Enter-VsDevShell -VsInstallPath $vsInstall -SkipAutomaticLocation -Arch amd64 -HostArch amd64 | Out-Null
$env:PATH+=';C:\Users\jiang\AppData\Local\Programs\Python\Python310;C:\Users\jiang\AppData\Local\pnpm\bin;C:\Program Files\nodejs;C:\Program Files\Git\cmd'
$env:VSLANG='1033'
$env:PYTHONDONTWRITEBYTECODE='1'
$env:PYTHONIOENCODING='utf-8'
$configName=$Configuration.ToLowerInvariant()
$preset='windows-msvc-headless-'+$configName
$prefix='out/094-main-'+$configName
$targets=@(
 'rynui_tree_component_tests','rynui_tree_model_tests','rynui_tree_theme_tests','rynui_tree_review_tests','rynui_tree_r2_tests',
 'rynui_auto_complete_component_tests','rynui_auto_complete_model_tests','rynui_auto_complete_theme_tests',
 'rynui_auto_complete_integration_tests','rynui_auto_complete_review_tests','rynui_auto_complete_r2_tests','rynui_auto_complete_r3_tests',
 'rynui_component_failure_retirement_tests','rynui_portable_focus_order','rynui_portable_focus_state','rynui_portable_focus_lifecycle',
 'rynui_portable_pointer_route','rynui_portable_input_component','rynui_portable_input_actions','rynui_portable_select_component',
 'rynui_portable_layout_style','rynui_portable_modal_component','rynui_popover_integration_tests','rynui_popover_review_tests',
 'rynui_time_picker_review_tests','rynui_date_picker_review_tests','rynui_form_component_tests',
 'rynui_avatar_component_tests','rynui_avatar_theme_tests',
 'rynui_statistic_model_tests','rynui_statistic_theme_tests','rynui_statistic_public_api_tests','rynui_statistic_component_tests',
 'rynui_rate_model_tests','rynui_rate_theme_tests','rynui_rate_public_api_tests','rynui_rate_component_tests',
 'rynui_rate_lifecycle_tests','rynui_rate_font_limit_tests','rynui_rate_focus_order_tests','rynui_rate_review_tests',
 'rynui_skeleton_component_tests','rynui_skeleton_integration_tests','rynui_portable_theme_algorithm','rynui_portable_theme_runtime')
$filter='^rynui\.(tree_.*|auto_complete_.*|component_failure_retirement|statistic_.*|rate_.*|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries)|avatar_.*|portable\.(focus_order|focus_state|focus_lifecycle|pointer_route|input_component|input_actions|select_component|layout_style|modal_component|theme_algorithm|theme_runtime)|popover_children|popover_review_(focus|escape)|time_picker_review_(focus|row_focus)|date_picker_review_form_reset|form_component)$'
$ErrorActionPreference='Continue'
$timer=[Diagnostics.Stopwatch]::StartNew()
switch($Stage){
 'probe'{
  & cmake --build --preset $preset --parallel 2 --target rynui_statistic_component_tests *> ($prefix+'-probe-build.log')
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  & ./out/build/windows-msvc-headless/tests/$Configuration/rynui_statistic_component_tests.exe rollback *> ($prefix+'-probe.log')
 }
 'configure'{ & cmake --preset windows-msvc-headless *> ($prefix+'-configure.log') }
 'build'{ & cmake --build --preset $preset --parallel 2 --target @targets *> ($prefix+'-build.log') }
 'export'{
  & ./out/build/windows-msvc-headless/tests/$Configuration/rynui_tree_theme_tests.exe ('out/094-main-'+$configName+'-tree-goldens') *> ($prefix+'-export-tree.log')
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  & ./out/build/windows-msvc-headless/tests/$Configuration/rynui_auto_complete_theme_tests.exe ('out/094-main-'+$configName+'-ac-goldens') *> ($prefix+'-export-ac.log')
 }
 'test'{
  $list=& ctest --preset $preset -R $filter --show-only=json-v1
  if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
  [IO.File]::WriteAllText((Join-Path (Get-Location) ($prefix+'-inventory.json')),($list -join "`n"),[Text.UTF8Encoding]::new($false))
  & ctest --preset $preset --parallel 2 --output-on-failure -R $filter --output-junit (Join-Path (Get-Location) ($prefix+'-ctest.xml')) *> ($prefix+'-test.log')
 }
}
$code=$LASTEXITCODE
$timer.Stop()
Write-Output ('Stage='+$Stage+' Configuration='+$Configuration+' exit='+$code+' seconds='+$timer.Elapsed.TotalSeconds)
if($Stage -eq 'test' -or $code -ne 0){Get-Content ($prefix+'-'+$Stage+'.log') -Tail 35}
exit $code
