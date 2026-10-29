param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [ValidateSet('build','test','bench','capacity-build','capacity-test')][string]$Stage='build', [string]$Label='before')
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
$taskPrefix="out/101-$Label-$Stage-$Configuration"
$taskTimer=[Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference='Continue'
switch($Stage) {
 'build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_glyph_replace_tests rynui_glyph_replace_benchmark rynui_portable_glyph_scene rynui_portable_text_scene_service rynui_portable_scene_backend rynui_portable_glyph_gpu_resources *> ($taskPrefix+'.log') }
 'test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^(rynui\.glyph_replace|rynui\.portable\.(glyph_scene|text_scene_service|scene_backend|glyph_gpu_resources))$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
 'bench' { $taskArgs=@('--iterations','200','--rounds','3'); if($Label -eq 'before') {$taskArgs+='--baseline'}; & ('.\out\build\windows-msvc-headless\tests\'+$Configuration+'\rynui_glyph_replace_benchmark.exe') @taskArgs *> ($taskPrefix+'.log') }
 'capacity-build' { & cmake --build --preset $taskPreset --parallel 2 --target rynui_portable_scene_backend rynui_portable_glyph_gpu_resources rynui_portable_logical_scene_packing *> ($taskPrefix+'.log') }
 'capacity-test' { & ctest --preset $taskPreset --parallel 2 --no-tests=error --output-on-failure -R '^rynui\.portable\.(scene_backend|glyph_gpu_resources|logical_scene_packing)$' --output-junit (Join-Path (Get-Location) ($taskPrefix+'.xml')) *> ($taskPrefix+'.log') }
}
$taskExit=$LASTEXITCODE
$taskTimer.Stop()
[ordered]@{stage=$Stage;label=$Label;configuration=$Configuration;exit=$taskExit;seconds=$taskTimer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($taskPrefix+'.json')
Write-Output "Stage=$Stage Label=$Label Configuration=$Configuration exit=$taskExit seconds=$($taskTimer.Elapsed.TotalSeconds)"
exit $taskExit
