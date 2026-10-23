# Card 容器与交互验收

2026-10-23，实际 Windows/MSVC x64，preset `windows-msvc-headless`，Debug/Release。

Card 已接入共同 WindowComponentServices，提供五个 typed 内容区域、默认/小号、outlined/borderless、hoverable、静态 loading。各 slot 一次挂载；正文可以组合真实 Input、Select、Button，Card 本身没有 Tab 停靠点。header 的 extra 取自然宽度、title 使用剩余宽度，长标题可换行；极窄/零可用高度与 border_width=0 均可同步。

所有视觉沿用 logical scene v3：背景、header/actions 背景、分隔线、占位和内侧边框复用既有 corner fill/outline effects，保留逻辑平移、透明度与祖先矩形裁剪；阴影使用共同 retained surface。没有新增 GPU ABI、backend 或上传路径，也不提供任意后代的圆角遮罩。

真实组件测试验证：

- 区域顺序、56/38 header、24/12正文 padding、窄宽度/空标题与 typed 富标题。
- 背景及内部 Button 的 hover、点击；4个实际子控件的 Tab 顺序，Card 不额外进入顺序。
- loading 隐藏正文 Input/Select、清理焦点/输入会话/弹层/命中；header/actions 保留操作能力；恢复后编辑内容、editor/区域 identity、组件/文字容量保持。
- 主题纯颜色变化不重测 Card 或正文；尺寸/变体/标题变化不重新执行任何用户 slot。
- 静止不持续请求帧、无无效效果更新，销毁后子控件、输入、命中、效果资源清理；loading 触发 blur 回调直接卸载整个 Card 的重入路径通过。

```text
Debug complete CTest: 106/106 passed (210.67s)
Release complete CTest: 106/106 passed (10.64s)
final Debug Card fixture: passed (0.35s)
clang-format 22.1.3: 475 owned files, 0 failures
OpenSpec doctor: healthy
OpenSpec strict: 59/59, exit 0
git diff --check: passed
```

这些是平台通用合同，Gallery 与 Windows/Linux 原生窗口证据在后续独立任务记录。
