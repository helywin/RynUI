$ErrorActionPreference = 'Continue'
$acR3Targets = @(
    'rynui_auto_complete_model_tests', 'rynui_auto_complete_theme_tests',
    'rynui_auto_complete_component_tests', 'rynui_auto_complete_integration_tests',
    'rynui_auto_complete_review_tests', 'rynui_auto_complete_r2_tests', 'rynui_auto_complete_r3_tests',
    'rynui_portable_focus_order', 'rynui_portable_focus_state', 'rynui_portable_focus_lifecycle',
    'rynui_portable_pointer_route', 'rynui_portable_input_component', 'rynui_portable_input_actions',
    'rynui_popover_integration_tests', 'rynui_popover_review_tests',
    'rynui_time_picker_review_tests', 'rynui_date_picker_review_tests',
    'rynui_form_component_tests', 'rynui_portable_select_component'
)
$acR3Regex = '^rynui\.(auto_complete_.*|portable\.focus_order|portable\.focus_state|portable\.focus_lifecycle|portable\.pointer_route|portable\.input_component|portable\.input_actions|popover_children|popover_review_focus|popover_review_escape|time_picker_review_focus|time_picker_review_row_focus|date_picker_review_form_reset|form_component|portable\.select_component)$'
foreach ($acR3Config in @('Debug', 'Release')) {
    & ./out/092-msvc.ps1 -Action build -Config $acR3Config -Targets $acR3Targets
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $acR3Preset = 'windows-msvc-headless-' + $acR3Config.ToLowerInvariant()
    & ctest --preset $acR3Preset --parallel 2 --output-on-failure --no-tests=error -R $acR3Regex 2>&1 | Tee-Object "out/092-R3-final-test-$acR3Config.log"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
