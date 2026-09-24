[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [double[]] $Scales = @(1.0, 1.25, 1.5, 2.0),

    [ValidateSet('Default', 'Dark', 'Compact')]
    [string] $Theme = 'Default'
)

$ErrorActionPreference = 'Stop'
if (-not $IsWindows -and $env:OS -ne 'Windows_NT') {
    throw 'Password acceptance must run on Windows.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$galleryExecutable = Join-Path $repositoryRoot `
    "out\build\windows-msvc\examples\$Configuration\rynui_token_gallery.exe"
if (-not (Test-Path -LiteralPath $galleryExecutable)) {
    throw "Token Gallery executable was not found: $galleryExecutable"
}
$outputDirectory = Join-Path $repositoryRoot 'out\acceptance\windows-password'
if ($Theme -ne 'Default') {
    $outputDirectory = Join-Path $outputDirectory $Theme.ToLowerInvariant()
}
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach ($scale in $Scales) {
    if ($scale -notin @(1.0, 1.25, 1.5, 2.0)) {
        throw "Unsupported Password acceptance scale: $scale"
    }
    $scaleText = $scale.ToString('0.##',
        [System.Globalization.CultureInfo]::InvariantCulture)
    $diagnosticsPath = Join-Path $outputDirectory "scale-$scaleText.txt"
    $stderrPath = Join-Path $outputDirectory "scale-$scaleText-stderr.txt"
    $screenshotPath = Join-Path $outputDirectory "scale-$scaleText.png"
    $arguments = @('--password-acceptance', "--acceptance-scale=$scaleText")
    if ($Theme -ne 'Default') {
        $arguments += "--selection-theme=$($Theme.ToLowerInvariant())"
    }

    $process = Start-Process -FilePath $galleryExecutable `
        -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot `
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
                    -Pattern 'password_acceptance_stage=5' -Quiet)) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($process.HasExited) {
            throw "Password Gallery exited before the screenshot at scale $scaleText."
        }
        if ((Get-Date) -ge $deadline) {
            throw "Password Gallery did not reach stage 5 at scale $scaleText."
        }
        & (Join-Path $PSScriptRoot 'capture-window.ps1') `
            -Title 'RynUI Ant Design Token Gallery' -OutputPath $screenshotPath `
            -ClientOnly | Out-Null
        if (-not $process.WaitForExit(15000)) {
            throw "Password Gallery did not exit at scale $scaleText."
        }
        if ($process.ExitCode -ne 0) {
            throw "Password Gallery exited with code $($process.ExitCode) at scale $scaleText."
        }
        $diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw
        foreach ($expected in @('password_acceptance=true',
                'password_hidden=true', 'password_pointer=true',
                'password_keyboard=true', 'password_disabled=true',
                'password_scroll=true', 'live_samples=33',
                'gpu_driver=direct3d12', 'shader_format=DXIL',
                'window_system=win32',
                "selection_theme=$($Theme.ToLowerInvariant())", 'exit_code=0')) {
            if (-not $diagnostics.Contains($expected)) {
                throw "Password Gallery diagnostics lack $expected at scale $scaleText."
            }
        }
        Write-Output "scale=$scaleText exit_code=0 screenshot=$screenshotPath"
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
        $process.Dispose()
    }
}
