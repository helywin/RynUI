param([ValidateSet('configure','build','focused','inventory','contracts','representative','forms','frame-contract')][string]$Stage)
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
$prefix='out/098-stage2-'+$Stage
$ErrorActionPreference='Continue'
$timer=[Diagnostics.Stopwatch]::StartNew()
switch($Stage){
 'configure'{ & cmake --preset windows-msvc *> ($prefix+'.log') }
 'build'{ & cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests *> ($prefix+'.log') }
 'focused'{ & ./out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --tree-select-only *> ($prefix+'.log') }
 'inventory'{ & ./out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only *> ($prefix+'.log') }
 'frame-contract'{ & ./out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --frame-contract-only *> ($prefix+'.log') }
 'forms'{ & ./out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only *> ($prefix+'.log') }
 'representative'{ & ./out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --tree-autocomplete-only *> ($prefix+'.log') }
 'contracts'{
  $filter='^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$'
  & ctest --preset windows-msvc-debug --parallel 2 --output-on-failure -R $filter --output-junit (Join-Path (Get-Location) 'out/098-stage2-contracts.xml') *> ($prefix+'.log')
 }
}
$code=$LASTEXITCODE
$timer.Stop()
[ordered]@{stage=$Stage;exit=$code;seconds=$timer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($prefix+'-result.json')
Write-Output ('Stage='+$Stage+' exit='+$code+' seconds='+$timer.Elapsed.TotalSeconds)
if($code -ne 0 -or $Stage -in @('inventory','contracts')){Get-Content ($prefix+'.log') -Tail 26}
exit $code
