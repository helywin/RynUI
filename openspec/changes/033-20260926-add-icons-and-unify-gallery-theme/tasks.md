# Tasks

## 1. 规划
- [x] 1.1 完成资源与架构调查、规划产物，运行本 change strict、全仓 strict、可用 doctor 与 diff check，以 `docs:` 提交

## 2. 平台通用图标基础
- [x] 2.1 锁定官方图标、许可证及 SHA256，加入可重现生成工具与内嵌轮廓；验证资源/生成合同，以 `feat:` 提交
- [x] 2.2 实现 typed Icon、主题/语义样式继承、字号/DPI 缓存与生命周期；Windows MSVC Debug 构建和图标/文字定向 CTest 验证后以 `feat:` 提交

## 3. 平台通用控件与导航
- [x] 3.1 接入 Password、Input clear、Search 默认图标及辅助操作主题/焦点反馈；受控、键盘、禁用与清理定向 CTest 通过后以 `feat:` 提交
- [x] 3.2 实现 Text Button 与状态样式，修复导航悬浮；统一 Gallery 主题入口和背景，增加顶栏亮暗切换；主题/Button/Gallery 定向 CTest 通过后以 `fix:` 提交
- [x] 3.3 全部组件子项可导航，增加稳定组件锚点、选中和筛选恢复；Gallery/viewport 定向 CTest 通过后以 `fix:` 提交
- [x] 3.4 滚动条轨道/滑块增加悬浮、按下/拖动及清理反馈；亮暗颜色、捕获、松开/cancel/失焦的定向测试通过后以 `fix:` 提交

## 4. Windows 验收
- [x] 4.1 Windows MSVC Release D3D12：亮/暗、导航悬浮/子项、图标/密码、两列与窄窗口、四种缩放连续拖动及输入回归，保存 GPU 图像与事件证据，以 `test:` 提交

## 5. 平台通用集成验收
- [x] 5.1 Windows MSVC Debug 完整 CTest、当前 strict、全仓 strict、可用 doctor、diff check；更新支持边界与证据，以 `test:` 提交
