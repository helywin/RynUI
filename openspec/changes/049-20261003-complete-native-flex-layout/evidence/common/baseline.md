# 基线与默认 Stretch 平台通用证据

2026-10-03，Windows/MSVC/Ninja Multi-Config，`windows-msvc-headless` Debug/Release 受影响布局、Flex、Space、Text、Typography、Input、Button、选择组件和 Theme 合同各 22/22 通过（7.64s/5.26s）。日志为 baseline-debug.log 与 baseline-release.log。

覆盖实际验证字体的混合字号、多行、真实 Input snapshot baseline、Button 标签和 Typography 的实际位置；margin/主题字体修订、intrinsic 缓存、上下最大范围、WrapReverse、vertical fallback、align-self、默认 Stretch、min/max、嵌套拉伸 Button、零尺寸和销毁。direction-only 复用 retained 测量，shape 次数与身份保持。基线依赖祖先发生内部对齐变更时重新测量。

旧 Gallery 导航/文档容器与 Button block 合同显式 Start，保留原有产品意图。`windows-msvc` Debug Flex/Gallery frame 2/2 与默认栈真窗口 `--smoke` 通过；完整多尺度平台验收由 Windows 独立任务记录。
