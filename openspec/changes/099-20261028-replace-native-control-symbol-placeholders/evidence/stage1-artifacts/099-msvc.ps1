param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [ValidateSet('build','test','gallery-build','inventory','contracts')][string]$Stage = 'build')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath 'D:/code/RynUI'
$taskInstaller = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer'
$env:PATH = "$taskInstaller;$env:PATH"
$taskVs = & (Join-Path $taskInstaller 'vswhere.exe') -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module (Join-Path $taskVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $taskVs -SkipAutomaticLocation -Arch amd64 -HostArch amd64 | Out-Null
$env:VSLANG = '1033'
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONIOENCODING = 'utf-8'
$taskPreset = 'windows-msvc-headless-' + $Configuration.ToLowerInvariant()
$taskPrefix = 'out/099-' + $Stage + '-' + $Configuration
$taskTimer = [Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference = 'Continue'
switch ($Stage) {
    'build' {
        & cmake --build --preset $taskPreset --parallel 2 --target rynui_portable_menu_component rynui_pagination_component_tests rynui_table_component_tests rynui_date_picker_component_tests rynui_portable_icon_component rynui_portable_icon_vector rynui_portable_focus_order rynui_portable_input_actions *> ($taskPrefix + '.log')
    }
    'test' {
        & ctest --preset $taskPreset --parallel 2 --output-on-failure -R '^rynui\.(pagination_component|table_component|date_picker_component|portable\.(menu_component|icon_component|icon_vector|focus_order|input_actions))$' --output-junit (Join-Path (Get-Location) ($taskPrefix + '.xml')) *> ($taskPrefix + '.log')
    }
    'gallery-build' {
        & cmake --build --preset ('windows-msvc-' + $Configuration.ToLowerInvariant()) --parallel 2 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests *> ($taskPrefix + '.log')
    }
    'inventory' {
        & ('.\out\build\windows-msvc\tests\' + $Configuration + '\rynui_token_gallery_frame_tests.exe') --inventory-only *> ($taskPrefix + '.log')
    }
    'contracts' {
        & ctest --preset ('windows-msvc-' + $Configuration.ToLowerInvariant()) --parallel 2 --output-on-failure -R '^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$' --output-junit (Join-Path (Get-Location) ($taskPrefix + '.xml')) *> ($taskPrefix + '.log')
    }
}
$taskExit = $LASTEXITCODE
$taskTimer.Stop()
[ordered]@{stage=$Stage;configuration=$Configuration;exit=$taskExit;seconds=$taskTimer.Elapsed.TotalSeconds;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath ($taskPrefix + '.json')
Write-Output "Stage=$Stage Configuration=$Configuration exit=$taskExit seconds=$($taskTimer.Elapsed.TotalSeconds)"
exit $taskExit
