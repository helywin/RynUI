# 自然宽度按钮的横向绘制边缘

- Windows MSVC `windows-msvc-headless-release`，Button 组件 CTest 通过。回归检查自然宽度按钮的小数布局、填充及阴影的绘制边框、交互命中和显式小数宽度。
- Windows MSVC `windows-msvc-release`，后台 D3D12、DPR 1.5。Select 回归完成全部 10 个检查点。初始状态两个示例按钮的 4 条定向扫描线消除了此前至少 4 个 RGB8 色阶的差异，变更前最大差 19。
- Web 观测按 Chromium 141 的像素取整边框得到绘制宽度，DOM 原始宽度继续用于布局判定。分层回归 47 项通过。完整 Select 和三例 paired 仍待通过。[Chromium 141 边框计算](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/contoured_border_geometry.cc#L160)
