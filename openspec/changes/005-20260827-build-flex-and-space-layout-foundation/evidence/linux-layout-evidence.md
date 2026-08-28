schema_version=1
change=005-20260827-build-flex-and-space-layout-foundation
platform=linux
status=pending
preset_debug=linux-gcc-debug
preset_release=linux-gcc-release
build_system=Ninja Multi-Config
compiler=pending
cpp_standard=c++20
dependency_mode=BUNDLED
window_system=pending
display_scale=pending
pixel_density=pending
window_size_wide=pending
window_size_narrow=pending
pixel_size_wide=pending
pixel_size_narrow=pending
viewport_wide=pending
viewport_narrow=pending
dpi_scale_applied=pending
font_logical_pixel_size=pending
font_raster_pixel_size=pending
font_raster_scale=pending
font_source=pending
font_families=pending
gpu_driver=pending
shader_format=SPIR-V
exit_code=pending
screenshot_wide_path=evidence/linux-layout-wide-pending.png
screenshot_narrow_path=evidence/linux-layout-narrow-pending.png
line_count_wide=pending
line_count_narrow=pending
content_runs=pending
prop_updates=pending
component_count=pending
layout_passes=pending
scene_rebuilds=pending
frame_submissions=pending
idle_waits=pending

## 2026-08-28 平台通用自动验收

以下结果只完成 tasks 6.1 与 6.2，不表示原生 Wayland 真实窗口验收通过：

```text
linux-gcc configure=clean
linux-gcc compiler=GNU 16.2.1
linux-gcc generator=Ninja Multi-Config
linux-gcc dependency_mode=BUNDLED
linux-gcc fontconfig=2.18.3
linux-gcc cpp_flags=-std=c++20
linux-gcc Debug build=passed
linux-gcc Debug CTest=127/127 passed
linux-gcc Release build=passed
linux-gcc Release CTest=127/127 passed

linux-clang configure=clean
linux-clang compiler=Clang 22.1.8
linux-clang generator=Ninja Multi-Config
linux-clang dependency_mode=BUNDLED
linux-clang cpp_flags=-std=c++20
linux-clang Debug build=passed
linux-clang Debug CTest=127/127 passed
```

生成的 GCC 与 Clang Ninja rules 使用 `-std=c++20`，未使用 `-std=gnu++20`。依赖锁、shader 生成/部署、public dependency、header isolation、Flex/Space/LayoutStyle、Fontconfig、frame 与 evidence schema 均包含在完整 CTest 中。

原生 Wayland task 6.3 因窗口 resize 阻塞保持未完成，统一状态见 [docs/open-issues.md](../../../../docs/open-issues.md)，详细诊断见 [docs/issues/linux-wayland-resize.md](../../../../docs/issues/linux-wayland-resize.md)。
