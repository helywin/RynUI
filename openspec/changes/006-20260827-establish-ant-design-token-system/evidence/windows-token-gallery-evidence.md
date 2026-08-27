schema_version=1
change=006-20260827-establish-ant-design-token-system
platform=windows
status=passed
preset_debug=windows-msvc-debug
preset_release=windows-msvc-release
build_system=Ninja Multi-Config
compiler=MSVC 19.51.36256.0 x64
display_scale=1.0,1.5,2.0
host_display_scale=1.5
scale_source=acceptance-render-override
window_system=win32
gpu_driver=NVIDIA GeForce RTX 5080 via SDL3 direct3d12
driver_version=32.0.16.1047
shader_format=DXIL
shader_hash=e215b3f8ae63ddad59fd0e21c574e0306407ab9886858df3ff4b5cde78932ada
shader_vertex_hash=91f8dc0303fabf173040590ecfb30be99e89219ca9b8f73b58f63bc70c567236
catalog_hash=b7e1e43a9400c42a7e545a0afa1b0a3117e1a352e2970c27870ac58e9181cd25
theme_states=Default,Dark,Compact,Brand Seed,Nested Theme
interaction_states=hover,active,focus-visible,disabled,loading
screenshot_primary_path=evidence/windows-token-gallery-100.png
screenshot_secondary_path=evidence/windows-token-gallery-150.png
screenshot_tertiary_path=evidence/windows-token-gallery-200.png
effect_layers=110
outer_layers=55
inset_layers=4
focus_layers=51
quad_uploads=81
glyph_uploads=182
effect_uploads=83
quad_draws=306
glyph_draws=312
effect_draws=330
frame_submissions=6
idle_waits=154
exit_code=0

验收环境：Windows 11 10.0.26200，`windows-msvc` clean configure，`Ninja Multi-Config`，MSVC x64；Debug 与 Release 均完成构建，Windows/D3D12/DXIL 专属测试通过。上述 counters 取自 150% 运行；100%、150%、200% 三次 smoke 均独立返回 `exit_code=0`。

缩放边界：宿主显示器在本次验收期间保持 150%。三个真实 D3D12/DXIL 窗口通过 `--acceptance-scale=1.0|1.5|2.0` 分别驱动 viewport、系统字体 raster 与 RoundedEffect device conversion；这验证的是三档 RynUI render scale，不伪装成三次 Windows 全局显示设置变更。

人工检查：三档截图均使用 Windows Graphics Capture 保存。Default/Dark/Compact、brand Seed 与 nested Theme 由真实窗口交互或 smoke 状态机切换；hover/active/focus-visible/disabled/loading 均进入 retained 更新路径。三档 elevation、Button/Drawer/Popover/Card/Tabs outer/inset shadow 未见硬边、异常加深或内容侧裁切；150% Compact 截图保留键盘 focus-visible，显示 1 logical px 透明 gap 与 3 logical px hollow ring。响应式 Gallery 保持标签单行、左右留白一致，200% 可见区域内字体与圆角边缘清晰。
