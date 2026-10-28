param([string]$Action = 'configure', [string]$Config = 'Debug', [switch]$Final, [string[]]$Targets = @('rynui_auto_complete_model_tests', 'rynui_auto_complete_theme_tests', 'rynui_auto_complete_component_tests', 'rynui_auto_complete_integration_tests'))
$ErrorActionPreference = 'Stop'
Import-Module 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Enter-VsDevShell -VsInstallPath 'C:/Program Files/Microsoft Visual Studio/18/Community' -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH = 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja;' + $env:PATH
$env:PYTHON = 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe'
$env:CLANG_FORMAT = 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
$preset = 'windows-msvc-headless-' + $Config.ToLowerInvariant()
$log = "out/092-$Action-$Config-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
$ErrorActionPreference = 'Continue'
if ($Final) { $Targets += @('rynui_portable_theme_algorithm','rynui_portable_theme_runtime','rynui_popover_integration_tests') }
if ($Action -eq 'configure') {
    $fontDir = Join-Path $PWD 'out/build/windows-msvc-headless/_deps/rynui-validation-fonts'
    New-Item -ItemType Directory -Force -Path $fontDir | Out-Null
    Copy-Item -LiteralPath 'D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSans-Regular.ttf' -Destination $fontDir
    Copy-Item -LiteralPath 'D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui-validation-fonts/NotoSansCJKsc-Regular.otf' -Destination $fontDir
    & cmake --preset windows-msvc-headless '-DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_freetype-src' '-DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_harfbuzz-src' '-DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_utf8proc-src' '-DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_sheenbidi-src' '-DPython3_EXECUTABLE=C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' 2>&1 | Tee-Object $log
} elseif ($Action -eq 'build') {
    & cmake --build --preset $preset --parallel 2 --target $Targets 2>&1 | Tee-Object $log
} elseif ($Action -eq 'test') {
    & ctest --preset $preset --parallel 2 --output-on-failure -R '^rynui\.(auto_complete_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips))$' 2>&1 | Tee-Object $log
} elseif ($Action -eq 'goldens') {
    & "out/build/windows-msvc-headless/tests/$Config/rynui_auto_complete_theme_tests.exe" "out/092-goldens-$Config" 2>&1 | Tee-Object $log
} else { throw "unknown action $Action" }
$code = $LASTEXITCODE
"exit=$code" | Tee-Object -Append $log
exit $code
