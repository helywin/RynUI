# Select 内置多选标签尺寸与材质

- Windows MSVC `windows-msvc-headless-release`，Input 绘制和变体以及 Select 组件共 3 项通过。多选用例检查 Middle 标签 56×24 逻辑像素，切换 Large 后原有标签节点保持，标签高 32。
- Windows MSVC `windows-msvc-release`，后台 D3D12、DPR 1.5。真实 Select 生命周期完成 10 个检查点。首个“桌面”标签外框从 68×22 变为 56×24，起始位置、4 dp 圆角、背景 alpha 0.06 与移除图标 alpha 0.45 均与锁定 Web 样例一致。
- 本次验证覆盖内置标签尺寸、材质和现有交互回归。完整 Select 像素差分仍待通过。
