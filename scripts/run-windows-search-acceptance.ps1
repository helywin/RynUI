[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [double[]] $Scales = @(1.0, 1.25, 1.5, 2.0)
)

$ErrorActionPreference = 'Stop'
if (-not $IsWindows -and $env:OS -ne 'Windows_NT') {
    throw 'Search acceptance must run on Windows.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$galleryExecutable = Join-Path $repositoryRoot `
    "out\build\windows-msvc\examples\$Configuration\rynui_token_gallery.exe"
if (-not (Test-Path -LiteralPath $galleryExecutable)) {
    throw "Token Gallery executable was not found: $galleryExecutable"
}
$outputDirectory = Join-Path $repositoryRoot 'out\acceptance\windows-search'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach ($scale in $Scales) {
    if ($scale -notin @(1.0, 1.25, 1.5, 2.0)) {
        throw "Unsupported Search acceptance scale: $scale"
    }
    $scaleText = $scale.ToString('0.##',
        [System.Globalization.CultureInfo]::InvariantCulture)
    $diagnosticsPath = Join-Path $outputDirectory "scale-$scaleText.txt"
    $stderrPath = Join-Path $outputDirectory "scale-$scaleText-stderr.txt"
    $screenshotPath = Join-Path $outputDirectory "scale-$scaleText.png"

    $process = Start-Process -FilePath $galleryExecutable `
        -ArgumentList @('--search-acceptance', "--acceptance-scale=$scaleText") `
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
                    -Pattern 'search_acceptance_stage=5' -Quiet)) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($process.HasExited) {
            throw "Search Gallery exited before the screenshot at scale $scaleText."
        }
        if ((Get-Date) -ge $deadline) {
            throw "Search Gallery did not reach stage 5 at scale $scaleText."
        }
        & (Join-Path $PSScriptRoot 'capture-window.ps1') `
            -Title 'RynUI Ant Design Token Gallery' -OutputPath $screenshotPath `
            -ClientOnly | Out-Null
        if (-not $process.WaitForExit(15000)) {
            throw "Search Gallery did not exit at scale $scaleText."
        }
        if ($process.ExitCode -ne 0) {
            throw "Search Gallery exited with code $($process.ExitCode) at scale $scaleText."
        }
        $diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw
        foreach ($expected in @('search_acceptance=true',
                'search_keyboard=true', 'search_pointer=true',
                'search_blocked=true', 'search_text=true', 'search_scroll=true',
                'search_submits=3', 'live_samples=33',
                'gpu_driver=direct3d12', 'shader_format=DXIL',
                'window_system=win32', 'exit_code=0')) {
            if (-not $diagnostics.Contains($expected)) {
                throw "Search Gallery diagnostics lack $expected at scale $scaleText."
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
