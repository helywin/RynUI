# Windows Input 自动窗口验收（任务 9.2）

scope=windows
status=passed-automated-only
source_sha=e00b5f1ec3b6332d7100d5119defb23e8ebc63f6
execution_date=2026-09-22
preset=windows-msvc-release
window_system=win32
gpu_driver=direct3d12
shader_format=DXIL
font_source=system
font_families=Segoe UI Variable Text,Microsoft YaHei UI
acceptance_scales=1.0,1.25,1.5,2.0
process_exit_codes=0,0,0,0
automated_checks=Latin,selection,system clipboard copy-cut-paste,undo,redo,Dark Theme,Warning status,caret session and deadline cleanup
manual_ime_visual_result=pending

无截图 runner：`scripts/run-windows-input-acceptance.ps1 -Configuration Release`。2026-09-22 在 Windows 11 x64 上依次启动四个真实 Token Gallery 窗口；每档的 `input_latin`、`input_selection`、`input_clipboard`、`input_undo`、`input_redo`、`input_theme_status`、`input_caret_idle` 均为 `passed`，stdout `exit_code=0` 与进程退出码一致。焦点丢失/恢复时同时同步 Component FocusManager 与 Input session；paste 后显式结束 history merge，使 undo/redo 结果不依赖渲染耗时。

stdout 原件只保存在本机 ignored 输出目录，不纳入仓库；文件与 SHA256：

- `out/acceptance/windows-input/scale-1.txt`：`8ddf3735fc131f32544369caf1b86fa05b9ed1f1c08305735d20d4e12bd72c7b`
- `out/acceptance/windows-input/scale-1.25.txt`：`3edd4b0ee6dd18e5fa53a06ea894dfb77baab1d8a4266105e54d9e6f1e5cbcd5`
- `out/acceptance/windows-input/scale-1.5.txt`：`38bcb55e4c77b43233100e2b489385a89bf64917d7a563d2d7f52e18cff21707`
- `out/acceptance/windows-input/scale-2.txt`：`27a97bb36ab631f91067f7a5ebb87e0b9ceea0ba0bfe349e4d6bc6c3edf899da`

`acceptance_scale` 是应用的受控渲染比例，不等于物理显示器 DPI；本机 stdout 同时记录了 `host_display_scale=1.5`。自动注入的是 RynUI 内部事件，不是系统中文 IME；没有人工确认候选窗、真实 DPI 无裁切或视觉质量，因此 `windows-input.md` 仍为 pending，任务 9.3/9.4 不由此关闭。
