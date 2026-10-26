$ErrorActionPreference = 'Continue'
$acReviewTargets = @(
    'rynui_auto_complete_model_tests', 'rynui_auto_complete_theme_tests',
    'rynui_auto_complete_component_tests', 'rynui_auto_complete_integration_tests',
    'rynui_auto_complete_review_tests', 'rynui_portable_theme_algorithm',
    'rynui_portable_theme_runtime', 'rynui_popover_integration_tests',
    'rynui_popover_review_tests', 'rynui_date_picker_component_tests',
    'rynui_date_picker_review_tests', 'rynui_time_picker_component_tests',
    'rynui_time_picker_review_tests', 'rynui_portable_input_component',
    'rynui_portable_input_actions', 'rynui_portable_input_count',
    'rynui_form_component_tests', 'rynui_portable_select_component'
)
$acReviewRegex = '^rynui\.(auto_complete_.*|portable\.theme_algorithm|portable\.theme_runtime|portable\.input_component|portable\.input_actions|portable\.input_count|portable\.select_component|popover_wheel|popover_children|popover_covered|popover_clips|popover_review_positioning|popover_review_focus|date_picker_component|date_picker_review_wheel|date_picker_review_form_reset|time_picker_component|time_picker_review_reset|time_picker_review_focus|form_component)$'
Copy-Item -LiteralPath out/092-R1-final-test-Debug.log -Destination out/092-R1-pre-autofocus-test-Debug.log
foreach ($acReviewConfig in @('Debug', 'Release')) {
    & ./out/092-msvc.ps1 -Action build -Config $acReviewConfig -Targets $acReviewTargets
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $acReviewPreset = 'windows-msvc-headless-' + $acReviewConfig.ToLowerInvariant()
    & ctest --preset $acReviewPreset --parallel 2 --output-on-failure --no-tests=error -R $acReviewRegex 2>&1 | Tee-Object "out/092-R1-final-test-$acReviewConfig.log"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
