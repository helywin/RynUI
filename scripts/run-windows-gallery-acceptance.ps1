[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [double[]] $Scales = @(1.0, 1.25, 1.5, 2.0)
)

$ErrorActionPreference = 'Stop'

if(-not $IsWindows -and $env:OS -ne 'Windows_NT') {
    throw 'Windows Reference Gallery acceptance must run on Windows.'
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$galleryExecutable = Join-Path $repositoryRoot `
    "out\build\windows-msvc\examples\$Configuration\rynui_token_gallery.exe"
if(-not (Test-Path -LiteralPath $galleryExecutable)) {
    throw "Reference Gallery executable was not found: $galleryExecutable"
}

$allowedScales = @(1.0, 1.25, 1.5, 2.0)
$outputDirectory = Join-Path $repositoryRoot `
    'out\acceptance\windows-reference-gallery'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

foreach($scale in $Scales) {
    if($scale -notin $allowedScales) {
        throw "Unsupported acceptance scale: $scale"
    }
    $scaleText = $scale.ToString(
        '0.##',
        [System.Globalization.CultureInfo]::InvariantCulture)
    $diagnosticsPath = Join-Path $outputDirectory "scale-$scaleText.txt"

    Write-Host "Reference Gallery acceptance scale: $scaleText"
    Write-Host 'Browse Introduction, Foundation, and the final item of all seven categories.'
    Write-Host 'Exercise category navigation, every support filter, wheel, Tab/Enter, and resize.'
    Write-Host 'Check Default/Primary/Danger hover, active, pointer focus, and keyboard focus.'
    Write-Host 'Close the Gallery window normally to continue to the next scale.'

    & $galleryExecutable "--acceptance-scale=$scaleText" 2>&1 |
        Tee-Object -FilePath $diagnosticsPath
    if($LASTEXITCODE -ne 0) {
        throw "Reference Gallery scale $scaleText exited with code $LASTEXITCODE."
    }
}

Write-Host "Diagnostics saved under: $outputDirectory"
