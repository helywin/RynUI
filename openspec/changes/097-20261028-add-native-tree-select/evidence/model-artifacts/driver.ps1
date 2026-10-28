param([ValidateSet('configure','build-model','test-model','build','test')][string]$Stage,
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
$prefix='out/097-'+$Configuration+'-'+$Stage
$timer=[Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference='Continue'
switch($Stage){
 'configure'{ & cmake --preset windows-msvc-headless *> ($prefix+'.log') }
 'build-model'{ & cmake --build --preset $preset --parallel 2 --target rynui_tree_select_model_tests *> ($prefix+'.log') }
 'test-model'{ & ctest --preset $preset --parallel 2 --output-on-failure -R '^rynui\.tree_select_model$' --output-junit (Join-Path (Get-Location) ($prefix+'.xml')) *> ($prefix+'.log') }
 'build'{ & cmake --build --preset $preset --parallel 2 --target rynui_tree_select_model_tests rynui_tree_select_component_tests *> ($prefix+'.log') }
 'test'{ & ctest --preset $preset --parallel 2 --output-on-failure -R '^rynui\.tree_select_.*$' --output-junit (Join-Path (Get-Location) ($prefix+'.xml')) *> ($prefix+'.log') }
}
$code=$LASTEXITCODE
$timer.Stop()
[ordered]@{stage=$Stage;configuration=$Configuration;exit=$code;seconds=$timer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($prefix+'-result.json')
Write-Output ('Stage='+$Stage+' Configuration='+$Configuration+' exit='+$code+' seconds='+$timer.Elapsed.TotalSeconds)
if($Stage -notin @('build-model','build') -or $code -ne 0){ Get-Content ($prefix+'.log') -Tail 30 }
exit $code
