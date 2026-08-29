# Animation Runtime 平台通用验收

schema_version=1
change=009-20260829-build-animation-runtime-foundation
scope=platform-generic
platform=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows NT 10.0.26200.0
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
clock_source=ControlledAnimationClock and controlled FrameEventSource
clock_resolution=integer-microseconds
cadence_hz=60,120,144
theme_source=Ant Design 6.5.0 locked token catalog
motion_tokens=fast:0.1s,mid:0.2s,slow:0.3s,easeInOut:cubic-bezier(0.645,0.045,0.355,1)
motion_preference=normal,reduced,theme-disabled
lifecycle_created=10000
lifecycle_completed=5000
lifecycle_canceled=5000
lifecycle_retargeted=10000
lifecycle_active=0
allocation_count=0
button_journey=hover,active,leave,loading,focus-visible,theme-toggle,reduced,gpu-deferred-retry,owner-destroy
spinner_segments=8
spinner_phase_wrap=passed
target_dirty_domains=Material,Animation
target_material_range_count=2
gpu_deferred_retries=1
frame_submissions=46
animation_frames=14
deferred_submissions=1
idle_waits=118
idle_after_animation=1
last_animation_idle=true
dependency_mode=BUNDLED
driver=not-required-platform-generic
shader_format=not-required-platform-generic
font_source=Noto Sans and Noto Sans CJK SC validation fixtures
real_window_evidence_path=not-required-platform-generic
unit_tests_exit_code=0
headless_tests_exit_code=0
contract_tests_exit_code=0
benchmark_tests_exit_code=0
openspec_validate_exit_code=0
openspec_doctor_healthy=true
git_diff_check_exit_code=0
exit_code=0

验收范围：受控时钟覆盖同 timestamp、倒退 timestamp、精确端点和 deterministic replay；deadline tests 覆盖 60/120/144 Hz、event coalescing、long stall、deferred retry 与最后一帧恢复 idle。Button headless journey 覆盖 pointer、keyboard focus、loading、Theme motion 开关、reduced policy、GPU upload retry 和 owner destroy。Spinner benchmark 以 256 个 Button、20,000 次 Material 更新验证固定 2,560 Quad 容量和 steady-state 0 heap allocation。

平台边界：本记录不验证 Windows D3D12/DXIL 或 Linux Wayland/Vulkan/SPIR-V 的真实窗口、display scale、driver、shader 和系统字体；这些结果必须由各自平台 evidence 独立完成。
