# 固定 Gallery 图标的原生光栅

- 显式启用的固定字体 Gallery 使用现有 Ant SVG 源及锁定 Skia 生成图标灰度覆盖，并沿用原生 glyph atlas 上传、材质与裁剪。浏览器控制板把图标放入独立的整视口绘制区域，避免官方 Loading 图层改变远处控制的光栅。正式 demo 的 DOM 和样式保持原样。
- `windows-msvc-release` hidden D3D12 与 `linux-gcc-release` private Wayland/Vulkan 各检查 80 个训练／留出图标。最大原生／浏览器覆盖质量相对误差分别为 0.0062 和 0.0067，局部覆盖误差分别为 0.0118 和 0.0091，轮廓距离分别为 0.084 和 0.067 physical px。两侧每组均通过独立 SVG 墨迹与净空检查。图标生成校验复现 848 个图标。
- 两平台的实际原生 Button、Input、Select 分别完成 4、3、10 个语义检查点，原生字体运行时 CTest 各 1/1，校准、区域与正式图标 Python 检查 41/41。
- 浏览器对照在 DPR 1.5 下保留训练和留出板各 40 个图标的像素，并恢复未与 Loading 共层时的阴影结果。圆角细描边及完整成对 pilot 仍待收口。
