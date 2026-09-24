# 平台通用回归

- 环境：Windows 11、Visual Studio 2026 MSVC，正式 `windows-msvc` Ninja Multi-Config preset 的 Debug 配置。
- 构建：`pwsh -NoProfile -File scripts/build-windows.ps1 -Configuration Debug -SkipTests`，退出码 0。
- 定向 CTest：`password_component`、`input_component`、`input_pointer`、`input_keyboard`、`search_component`、`token_gallery_frame`、`gallery_platform_generic_journey`，7/7 通过。
- 焦点 CTest：`text_input_focus`、`focus_order`、`focus_state`、`focus_lifecycle`、`focus_allocation`，5/5 通过。
- `git diff --check` 通过。此处只证明平台通用行为回归；真实 Win32 窗口与 Release 另行记录。
