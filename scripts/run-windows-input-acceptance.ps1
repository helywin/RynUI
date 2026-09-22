[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [double[]] $Scales = @(1.0, 1.25, 1.5, 2.0)
)

$ErrorActionPreference = 'Stop'

if(-not $IsWindows -and $env:OS -ne 'Windows_NT') {
    throw 'Windows Input acceptance must run on Windows.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$galleryExecutable = Join-Path $repositoryRoot `
    "out\build\windows-msvc\examples\$Configuration\rynui_token_gallery.exe"
if(-not (Test-Path -LiteralPath $galleryExecutable)) {
    throw "Token Gallery executable was not found: $galleryExecutable"
}

$allowedScales = @(1.0, 1.25, 1.5, 2.0)
$outputDirectory = Join-Path $repositoryRoot `
    'out\acceptance\windows-input'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach($scale in $Scales) {
    if($scale -notin $allowedScales) {
        throw "Unsupported acceptance scale: $scale"
    }
    $scaleText = $scale.ToString(
        '0.##',
        [System.Globalization.CultureInfo]::InvariantCulture)
    $diagnosticsPath = Join-Path $outputDirectory "scale-$scaleText.txt"

    Write-Host "Windows Input automated acceptance scale: $scaleText"
    $diagnostics = [System.Collections.Generic.List[string]]::new()
    & $galleryExecutable `
        '--input-acceptance' "--acceptance-scale=$scaleText" 2>&1 |
        ForEach-Object {
            $line = $_.ToString()
            $diagnostics.Add($line)
            $line
        } |
        Tee-Object -FilePath $diagnosticsPath
    $exitCode = $LASTEXITCODE
    if($exitCode -ne 0) {
        throw "Windows Input scale $scaleText exited with code $exitCode."
    }

    $text = $diagnostics -join [Environment]::NewLine
    foreach($required in @(
        'window_system=win32',
        'gpu_driver=direct3d12',
        'shader_format=DXIL',
        "display_scale=$scaleText",
        'scale_source=acceptance',
        'input_acceptance=true',
        'input_latin=passed',
        'input_selection=passed',
        'input_clipboard=passed',
        'input_undo=passed',
        'input_redo=passed',
        'input_theme_status=passed',
        'input_caret_idle=passed',
        'exit_code=0')) {
        if(-not $text.Contains($required)) {
            throw "Windows Input scale $scaleText diagnostics are missing: $required"
        }
    }
}

Write-Host "Diagnostics saved under: $outputDirectory"
