$ErrorActionPreference = 'Continue'
$acR2Targets = @(
    'rynui_auto_complete_model_tests', 'rynui_auto_complete_theme_tests',
    'rynui_auto_complete_component_tests', 'rynui_auto_complete_integration_tests',
    'rynui_auto_complete_review_tests', 'rynui_auto_complete_r2_tests',
    'rynui_portable_focus_order', 'rynui_portable_focus_state', 'rynui_portable_focus_lifecycle',
    'rynui_portable_input_component', 'rynui_portable_input_actions',
    'rynui_popover_integration_tests', 'rynui_popover_review_tests',
    'rynui_time_picker_review_tests', 'rynui_date_picker_review_tests',
    'rynui_form_component_tests', 'rynui_portable_select_component'
)
$acR2Regex = '^rynui\.(auto_complete_.*|portable\.focus_order|portable\.focus_state|portable\.focus_lifecycle|portable\.input_component|portable\.input_actions|popover_children|popover_review_focus|time_picker_review_focus|date_picker_review_form_reset|form_component|portable\.select_component)$'
foreach ($acR2Config in @('Debug', 'Release')) {
    & ./out/092-msvc.ps1 -Action build -Config $acR2Config -Targets $acR2Targets
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $acR2Preset = 'windows-msvc-headless-' + $acR2Config.ToLowerInvariant()
    & ctest --preset $acR2Preset --parallel 2 --output-on-failure --no-tests=error -R $acR2Regex 2>&1 | Tee-Object "out/092-R2-final-test-$acR2Config.log"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
