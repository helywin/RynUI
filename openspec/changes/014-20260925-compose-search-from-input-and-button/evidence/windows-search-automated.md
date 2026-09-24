# Search Windows 实窗自动流程（部分验收）

schema_version=1
change=014-20260925-compose-search-from-input-and-button
scope=windows-automated
status=partial
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
source_version=6.6.5
dependency_mode=BUNDLED
feature_commit_sha=c15c5d3d594d2cb8371b056c7c789ca192908c0c
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
host_display_scale=1.5
acceptance_scales=1,1.25,1.5,2
manual_visual_review=passed
native_ime=pending
source_contract_exit_code=0
search_component_exit_code=0
idle_benchmark_exit_code=0
screenshot_1_sha256=03841e9143018b074529654e094131d97d461bef98250f2d7a4998f2a9750d1b
screenshot_1_25_sha256=0f4c240713414193908df166de5f40d2413616f4048628eda6d39cc1da1c22e0
screenshot_1_5_sha256=2c45cdc19ac6347d73d1333f08d76f45a1a98942566571813cafbaeeb8ad23b1
screenshot_2_sha256=f3b583ab9fd5fe674fbf674cf22b21588c090bcba5dd72317f46cae3547b4e78
git_diff_check_exit_code=0
exit_code=0

`scripts/run-windows-search-acceptance.ps1 -Configuration Debug` 在真实 Win32 窗口覆盖四档模拟 scale，保存同名 PNG 与诊断日志。实际宿主显示缩放为 1.5；每档诊断均包含阶段 0–5、`search_keyboard=true`、`search_pointer=true`、`search_blocked=true`、`search_text=true`、`search_scroll=true`、三次提交和退出码 0。GPU 后端为 D3D12，shader 为 DXIL，系统字体链为 `Segoe UI Variable Text`、`Microsoft YaHei UI`。日志只证明后端，没有标识物理或虚拟 GPU 型号。

人工核对四张 client 区截图：Small/Middle/Large 的高度、Input 与 Button 相邻布局、中文和 Latin 文本、focus、loading/disabled、搜索结果文字在视口内清晰且没有相互裁剪或覆盖。按钮指针激活与 Enter/Space/Tab 的焦点流程由窗口运行时诊断确认；旧 Input 与 Selection 脚本各在 1.0 档回归通过。

自动流程向现有文本会话注入已提交的中文事件，不能证明 Windows 原生 IME 候选窗、composition 与真实输入法交互。Release 构建/CTest、原生 IME 和最终 Windows passed evidence 仍待验；因此本报告明确为 `partial`，不勾选 tasks.md 的 5.1–5.3。
