# Token Gallery 平台通用验收

schema_version=1
change=006-20260827-establish-ant-design-token-system
scope=platform-generic
status=passed
os=Microsoft Windows 11 专业工作站版 10.0.26200 (Build 26200)
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
catalog_hash=b7e1e43a9400c42a7e545a0afa1b0a3117e1a352e2970c27870ac58e9181cd25
catalog_version=Ant Design 6.5.0
stable_test_ids=51
catalog_identity_coverage=exact
wide_viewport=1200x900
narrow_viewport=560x1100
theme_content_runs_initial=52
theme_content_runs_final=52
theme_updates=4
brand_updates=1
state_updates=2
focus_gap_logical_px=1
focus_ring_logical_px=3
shadow_lists=17
effect_layers_benchmark=36
unit_tests_exit_code=0
headless_tests_exit_code=0
contract_tests_exit_code=0
benchmark_tests_exit_code=0
openspec_validate_exit_code=0
openspec_doctor_healthy=true
git_diff_check_exit_code=0
exit_code=0

验收说明：`rynui.token_gallery_frame` 在宽、窄 viewport 中验证全部 cell 具有可访问的非零边界，并核对 Theme content 在算法、品牌 Seed、状态和 viewport 更新期间不重跑。测试逐层比对三档 elevation、Button 三类 shadow、Drawer 四方向、Popover/Card 与 Tabs inset 的 kind、offset、blur、spread、color 和声明顺序；同时验证 keyboard focus 为 1 logical px 透明 gap 加 3 logical px hollow ring。

局部更新说明：headless frame 在初始帧完成 Quad、Glyph 与 RoundedEffect upload/draw；Theme 更新只触发 retained range upload，组件数量和 scene topology 保持稳定；40 次稳定轮询没有新增 submit。

平台边界：本记录只覆盖平台通用 unit、headless、contract 与 benchmark。Windows D3D12/DXIL 和 Linux Vulkan/SPIR-V 的真实 GPU、display scale、截图和人工视觉结果分别记录在独立 evidence 中，不由本记录替代。
