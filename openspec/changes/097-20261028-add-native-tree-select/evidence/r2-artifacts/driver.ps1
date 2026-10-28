param([ValidateSet('build','before','focused','combined-build','combined-test')][string]$Stage,
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
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONDONTWRITEBYTECODE='1'
$preset='windows-msvc-headless-'+$Configuration.ToLowerInvariant()
$prefix='out/097-r2-'+$Configuration+'-'+$Stage
$regex='^rynui\.(tree_.*|popover_.*|form_(model|public_api|component|theme)|auto_complete_r3_.*|component_failure_retirement|time_picker_review_focus|date_picker_review_form_reset|portable\.(focus_order|focus_state|focus_lifecycle|pointer_route|input_component|select_component|theme_runtime|theme_algorithm|modal_component))$'
$timer=[Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference='Continue'
switch($Stage){
 'build'{ & cmake --build --preset $preset --parallel 2 --target rynui_tree_select_r2_tests *> ($prefix+'.log') }
 'before'{ & ctest --preset $preset --parallel 2 --output-on-failure -R '^rynui\.tree_select_r2_.*$' --output-junit (Join-Path (Get-Location) ($prefix+'.xml')) *> ($prefix+'.log') }
 'focused'{ & ctest --preset $preset --parallel 2 --output-on-failure -R '^rynui\.tree_select_.*$' --output-junit (Join-Path (Get-Location) ($prefix+'.xml')) *> ($prefix+'.log') }
 'combined-build'{
   $targets=@('rynui_tree_select_model_tests','rynui_tree_select_component_tests','rynui_tree_select_theme_tests','rynui_tree_select_integration_tests','rynui_tree_select_review_tests','rynui_tree_select_r2_tests',
     'rynui_tree_model_tests','rynui_tree_component_tests','rynui_tree_theme_tests','rynui_tree_review_tests','rynui_tree_r2_tests',
     'rynui_popover_model_tests','rynui_popover_component_tests','rynui_popover_theme_tests','rynui_popover_public_api_tests',
     'rynui_popover_examples_tests','rynui_popover_integration_tests','rynui_popover_review_tests',
     'rynui_form_model_tests','rynui_form_component_tests','rynui_form_theme_tests','rynui_form_public_api_tests',
     'rynui_auto_complete_r3_tests','rynui_component_failure_retirement_tests','rynui_time_picker_review_tests','rynui_date_picker_review_tests',
     'rynui_portable_focus_order','rynui_portable_focus_state','rynui_portable_focus_lifecycle','rynui_portable_pointer_route',
     'rynui_portable_input_component','rynui_portable_select_component','rynui_portable_theme_runtime','rynui_portable_theme_algorithm','rynui_portable_modal_component')
   & cmake --build --preset $preset --parallel 2 --target @targets *> ($prefix+'.log')
 }
 'combined-test'{ & ctest --preset $preset --parallel 2 --output-on-failure -R $regex --output-junit (Join-Path (Get-Location) ($prefix+'.xml')) *> ($prefix+'.log') }
}
$code=$LASTEXITCODE
$timer.Stop()
[ordered]@{stage=$Stage;configuration=$Configuration;exit=$code;seconds=$timer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($prefix+'-result.json')
Write-Output ('Stage='+$Stage+' Configuration='+$Configuration+' exit='+$code+' seconds='+$timer.Elapsed.TotalSeconds)
if($Stage -ne 'build' -or $code -ne 0){ Get-Content ($prefix+'.log') -Tail 32 }
exit $code
