# Windows 固定字体的字形光栅与回读

- Windows MSVC `windows-msvc-headless-debug` 和 `windows-msvc-headless-release` 分别运行 `ctest -R '^rynui\.portable\.(font_runtime|scene_backend)$'`，各通过 2 项。
- `windows-msvc-release` 的 shader contract、layout 和 closure 测试 7/7 通过。
- `windows-msvc-release` 构建 Gallery 和校准程序。在隐藏窗口的 D3D12/DXIL 路径对照固定 Chromium 141.0.7390.37，训练板与独立留出板各 192 个字形逐像素一致。初次留出板仍有 56 处字形像素差异，最终修正允许字形墨迹越过排版框，保留显式裁字故障。
- 同一 Gallery 的 Button、Input、Select 分别完成 4、3、10 个原生检查点，独立行为断言无失败。
- 固定字体路径从 DirectWrite 三通道取平均，向原有 R8 atlas 提交覆盖值。GPU 根据文字颜色应用覆盖率转换。三通道覆盖模型可参照 [固定版 Skia 实现](https://raw.githubusercontent.com/google/skia/5eefbe51d17d2e379fa2d7353827e0ccb1e1f601/src/ports/SkScalerContext_win_dw.cpp)。
- 平台校准、配对 pilot 和 meta 验收继续追踪。未 hint 的设计轮廓在固定浏览器截图上也产生了误报，已定位为独立字形见证的几何问题。
