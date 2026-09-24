[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',
    [ValidateSet('Default', 'Dark')]
    [string[]] $Themes = @('Default', 'Dark')
)

$ErrorActionPreference = 'Stop'
if (-not $IsWindows -and $env:OS -ne 'Windows_NT') {
    throw 'Input clear acceptance must run on Windows.'
}
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$galleryExecutable = Join-Path $repositoryRoot `
    "out\build\windows-msvc\examples\$Configuration\rynui_token_gallery.exe"
if (-not (Test-Path -LiteralPath $galleryExecutable)) {
    throw "Token Gallery executable was not found: $galleryExecutable"
}
$outputDirectory = Join-Path $repositoryRoot 'out\acceptance\windows-input-clear'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach ($theme in $Themes) {
    $themeName = $theme.ToLowerInvariant()
    $diagnosticsPath = Join-Path $outputDirectory "$themeName-scale-1.5.txt"
    $stderrPath = Join-Path $outputDirectory "$themeName-scale-1.5-stderr.txt"
    $screenshotPath = Join-Path $outputDirectory "$themeName-scale-1.5.png"
    $arguments = @('--input-clear-acceptance', '--acceptance-scale=1.5')
    if ($theme -ne 'Default') { $arguments += '--selection-theme=dark' }
    $process = Start-Process -FilePath $galleryExecutable `
        -ArgumentList $arguments -WorkingDirectory $repositoryRoot `
        -RedirectStandardOutput $diagnosticsPath `
        -RedirectStandardError $stderrPath `
        -WindowStyle Normal -PassThru
    try {
        $deadline = (Get-Date).AddSeconds(40)
        while ((Get-Date) -lt $deadline) {
            $process.Refresh()
            if ($process.HasExited) { break }
            if ((Test-Path -LiteralPath $diagnosticsPath) -and
                (Select-String -LiteralPath $diagnosticsPath `
                    -Pattern 'input_clear_acceptance_stage=5' -Quiet)) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($process.HasExited) { throw "Input clear Gallery exited before screenshot: $theme" }
        if ((Get-Date) -ge $deadline) { throw "Input clear Gallery did not reach stage 5: $theme" }
        & (Join-Path $PSScriptRoot 'capture-window.ps1') `
            -Title 'RynUI Ant Design Token Gallery' -OutputPath $screenshotPath `
            -ClientOnly | Out-Null
        if (-not $process.WaitForExit(15000)) { throw "Input clear Gallery did not exit: $theme" }
        if ($process.ExitCode -ne 0) { throw "Input clear Gallery exited with code $($process.ExitCode): $theme" }
        $diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw
        foreach ($expected in @('input_clear_acceptance=true',
                'input_clear_scroll=true', 'input_clear_pointer=true',
                'input_clear_keyboard=true', 'input_clear_disabled=true',
                'live_samples=33', 'gpu_driver=direct3d12',
                'shader_format=DXIL', 'window_system=win32',
                "selection_theme=$themeName", 'exit_code=0')) {
            if (-not $diagnostics.Contains($expected)) {
                throw "Input clear diagnostics lack $expected for $theme"
            }
        }
        if ((Get-Item -LiteralPath $stderrPath).Length -ne 0) {
            throw "Input clear Gallery reported stderr for $theme"
        }
        Write-Output "theme=$themeName exit_code=0 screenshot=$screenshotPath"
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.Dispose()
    }
}
