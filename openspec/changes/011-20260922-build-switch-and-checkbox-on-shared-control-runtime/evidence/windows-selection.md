# Switch/Checkbox Windows 实窗验收

schema_version=1
change=011-20260922-build-switch-and-checkbox-on-shared-control-runtime
scope=windows
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug,windows-msvc-release
build_system=Ninja Multi-Config
cpp_standard=C++20
source_version=6.6.5
dependency_mode=BUNDLED
feature_commit_sha=d9c682df7b9bdb6deb411f7ffe3ca69e155c76d3
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
host_display_scale=1.5
acceptance_scales=1,1.25,1.5,2
ctest_debug=216/216
ctest_release=216/216
source_contract_exit_code=0
selection_component_exit_code=0
idle_benchmark_exit_code=0
manual_visual_review=passed
screenshot_1_sha256=ff6f02363d7bcc6226a8b46e4bb200efde8796e24b3820a21f562cb22befe47c
screenshot_1_25_sha256=1ed262826c9b5ec7875df8a288e629ddfb5dbe4e6ca938bee97c1f81d22d8b94
screenshot_1_5_sha256=c97110c21a0f100b9d192609e4641bcb5d72e0f79908c49b5d92c92be0f7c3eb
screenshot_2_sha256=d135b34573205ae11a7f8a53f5a77a90ee184c66b18e182b3a184e0beec2bc33
git_diff_check_exit_code=0
exit_code=0

2026-09-25，在 Windows 11 x64 上使用正式 `windows-msvc` Ninja Multi-Config、MSVC x64、BUNDLED 依赖完成 Debug/Release 构建与全量 CTest，各 216/216 通过。新增 evidence contract 后的 Debug 定向 CTest 14/14 通过，覆盖 Windows passed evidence、跨 scope 拒绝、shader、依赖锁与 license 字段、Python cache、公开 API 和组件回归。

`scripts/run-windows-selection-acceptance.ps1 -Configuration Debug` 在真实 Win32 窗口运行四档模拟 acceptance scale，实际宿主显示缩放为 1.5。每档诊断均记录阶段 0–4、`selection_keyboard=true`、`selection_pointer=true`、`selection_blocked=true`、正常退出码 0，以及 D3D12/DXIL、系统字体 `Segoe UI Variable Text` 与 `Microsoft YaHei UI`。交互阶段覆盖 Button/Input 混排下的 Tab、Switch/Checkbox 的 Space 激活与 Enter 门禁、Small Switch 指针、disabled/loading 禁止激活。日志没有标识 D3D12 所用物理或虚拟显卡，故此处只确认后端。

人工核对 `screenshots/windows-scale-{1,1.25,1.5,2}.png`：Switch Middle/Small 的轨道与滑块、loading 指示、Checkbox 的勾选与居中 indeterminate 方块、禁用态、键盘 focus ring、CJK/Latin 文本和目标控件的裁剪均可见且正常。截图是滚动到真实样例区后的窗口 client 区域；各档诊断保存在同名 `.txt`。

Linux 原生 Wayland、Vulkan/SPIR-V、Fontconfig 和实窗输入验收仍待独立执行。
