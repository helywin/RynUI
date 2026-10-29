param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [string]$Stage='build', [string]$Label='baseline', [string]$Component='reactive', [int]$Iterations=64, [int]$Warmup=8, [int]$Round=1)
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
$taskPrefix="out/103-$Label-$Stage-$Configuration"
if($Stage -eq 'bench') { $taskPrefix += "-$Component-r$Round" }
$taskTimer=[Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference='Continue'
switch($Stage) {
 'configure' { & cmake --preset windows-msvc-headless *> ($taskPrefix+'.log') }
 'build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_reactive_fanout_benchmark rynui_effect_range_benchmark rynui_memo_tests rynui_batch_tests rynui_scope_effect_tests rynui_portable_reactive_source_lifecycle rynui_portable_rounded_effect_math rynui_portable_rounded_effect_store rynui_portable_rounded_effect_scene rynui_portable_rounded_effect_gpu rynui_portable_rounded_effect_gpu_resources rynui_portable_scene_backend *> ($taskPrefix+'.log') }
 'test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.(reactive_fanout_contract|reactive_subscription_.*|effect_range_contract|signal|memo|batch|scope_effect|portable\.(reactive_source_lifecycle|rounded_effect_(math|store|scene|gpu|gpu_resources)|scene_backend|backend_boundaries))$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'subscription-build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_signal_tests rynui_reactive_subscription_tests *> ($taskPrefix+'.log') }
 'subscription-test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.(signal|reactive_subscription_.*)$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'bench' {
  $taskExe=if($Component -eq 'reactive') {'rynui_reactive_fanout_benchmark.exe'} else {'rynui_effect_range_benchmark.exe'}
  & ('.\out\build\windows-msvc-headless\tests\'+$Configuration+'\'+$taskExe) --iterations $Iterations --warmup $Warmup --rounds 1 1> ($taskPrefix+'.csv') 2> ($taskPrefix+'.log')
 }
 default { throw "Unknown stage: $Stage" }
}
$taskExit=$LASTEXITCODE
$taskTimer.Stop()
[ordered]@{stage=$Stage;label=$Label;configuration=$Configuration;component=$Component;iterations=$Iterations;warmup=$Warmup;round=$Round;exit=$taskExit;seconds=$taskTimer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($taskPrefix+'.json')
Write-Output "Stage=$Stage Label=$Label Configuration=$Configuration Component=$Component Round=$Round exit=$taskExit seconds=$($taskTimer.Elapsed.TotalSeconds)"
exit $taskExit
