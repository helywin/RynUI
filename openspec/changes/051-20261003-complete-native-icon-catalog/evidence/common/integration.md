# 图标 Gallery 与全量回归

2026-10-03，Windows / MSVC / Ninja Multi-Config。

- 完整 `windows-msvc-headless` Debug/Release build + CTest：75/75，105.12 / 19.13 秒。包括平台通用组件、字体/场景/上传/动画、typed API、分配与 backend boundaries 合同。
- `windows-msvc` 的纯逻辑 Gallery frame 集成测试 Debug/Release 各 1/1，23.67 / 6.20 秒。目录状态来自真实实现；图标现为 implemented，Web DOM/SVG parser/network API 在原生范围外。
- Gallery 添加八个样例：outlined、filled、默认双色、四层自定义双色、保留 source+rotate、可启停 spin、Quadratic/Cubic/holes 向量、偏移非正方形视框。三个操作按钮提供实际交互；初始 spin 为关闭，用户可启用。
- 实际计数为 118 stable test ids、137 live samples、60 Theme content runs；现有 73 目录项、126 reference surfaces/contents 和既有控件索引保留。
- Gallery 生成器 write/self-test/check、clang-format 22.1.3 check 429 文件、OpenSpec doctor healthy/strict 51/51、diff check 通过。

日志位于开发机 `out/icon-gallery-full-{debug,release}.log`、`out/icon-gallery-common-{debug,release}.log` 和 `out/icon-gallery-{doctor,openspec}.log`。本阶段仅记录逻辑集成；D3D12/DXIL 真窗口缩放/readback 在独立 Windows 阶段验收。
