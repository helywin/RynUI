# 固定字体的 Linux 光栅与独立字形见证

- `linux-gcc-release` 启用锁定 Fontations 与 Skia A8，构建 Gallery 和校准程序。私有 Mutter Wayland/Vulkan/SPIR-V 的训练与留出板共 384 个文字控制均通过独立空白与实墨见证。最大覆盖量相对误差 0.00727，最大投影相对误差 0.00767，最大轮廓距离 0.529 physical px，最大排版差 0.488 logical px。
- `windows-msvc-release` 隐藏 D3D12/DXIL 读回同类控制。384 个文字控制通过独立见证，覆盖量、投影与轮廓差均为零，最大排版差 0.016 logical px。两平台使用各自的固定 Chromium 诊断板，保留原有误差上限。
- Linux、Windows Release shader 合同各 7/7。Windows MSVC HEADLESS Debug 与 Release 的 `font_runtime` 和 `scene_backend` 各 2/2。校准定向 Python 22/22。两平台各完成原生 Button、Input 与 Select 的 17 个检查点，独立行为断言均通过。
- Linux 光栅在固定字体验收构建中显式启用。完整重复校准仍需解决图标和阴影差异，配对 pilot 与 meta 验收继续追踪。
