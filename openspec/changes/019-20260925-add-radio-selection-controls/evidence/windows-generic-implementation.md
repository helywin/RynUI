# Radio 平台通用实现验证

- 实际机器：Windows 10.0.26200，x64；Visual Studio 2026 MSVC 14.51.36231。
- 正式 preset：`windows-msvc-debug`，CMake `Ninja Multi-Config`。`scripts/build-windows.ps1 -Configuration Debug -SkipTests` 成功。
- 完整 Debug CTest：229/229 通过，220.18 秒。随后抽取共享标签挂载/资源清理并补强 Group 键盘测试，再用正式 Debug 构建及受影响 CTest 验证最终代码：`radio_public_api`、`radio_component`、`selection_component`、`selection_idle_benchmark`、`search_component`、`search_idle_benchmark`、`input_journey`、`token_gallery_frame`，8/8 通过。
- `radio_component` 覆盖独立和 Group 受控/未受控、重复选择、Space/Enter/Tab、指针、disabled、唯一值与模式冲突、回调自毁、四档模拟 font scale、圆点几何、Default/Dark/Compact、颜色最小材质失效与 sibling identity。Gallery frame 覆盖 Button/Input/Search/Switch/Checkbox/Radio 同窗挂载。
- `scripts/run-windows-selection-acceptance.ps1 -Configuration Debug -Scales 1.5` 在真实 Win32/D3D12 窗口退出码 0；截图 `out/acceptance/windows-selection/scale-1.5.png` 已查看，Radio 的 CJK/Latin 标签、圆环/圆点、互斥选中及 disabled 外观可见。此项为开发期抽检，四档 Release 证据在 Windows 专属阶段记录。
- 本次不覆盖原生中文 IME 候选/合成，也不把 Windows 结果视为 Linux Wayland/Vulkan 验收。
