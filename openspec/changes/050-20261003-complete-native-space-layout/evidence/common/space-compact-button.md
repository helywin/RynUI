# SpaceCompact / Button 阶段验收

2026-10-03，Windows/MSVC，`windows-msvc-headless` Debug/Release 受影响 CTest 各 15/15，3.54 / 2.01 秒。日志为工作区 `out/space-compact-{debug,release}.log`。

覆盖 typed API、H/V/RTL、三个 ControlSize、显式 Middle 优先、block/vertical 自动宽度拉伸、nested 外角与尺寸、空/单项/空嵌套、纯方向不重复测量、六 Button 变体、虚线、hover/focus/disabled 状态边优先、嵌套状态边、按角阴影数学覆盖、加载图标在背景之后、有限 wave/idle、捕获中删除和 resource 归零。

共有边用实际控件 border width 重叠，独立 after-children logical fragment 重绘状态优先边；虚线复用获胜控件的实际 decoration clips。嵌套组传递各子控件边框，避免丢失 hover/focus 状态。新按角 shadow helpers 复用共同 GPU ABI，原有非按角 surface 保持单个 shadow 实例。

Tooltip 浮层隔离祖先 Compact，浮层内部的独立 Compact 仍可继承；隐藏 Button 不读取尚未放置的 content geometry，并暂停 spinner/wave，显示后恢复 loading。测试包含实际打开/关闭及最终无持续 deadline。

这属于平台通用合同；Input/Password/Search/RadioButton/Addon 尚待下一阶段，Windows 真窗口/GPU 与 Linux 平台验收保持独立任务。
