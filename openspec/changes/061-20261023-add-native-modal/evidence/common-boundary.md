# 模态输入边界阶段验证

2026-10-23，实际 Windows/MSVC x64，`windows-msvc-headless`，Ninja Multi-Config。

实现逻辑组件子树输入边界，命中、焦点/ref、编辑会话和滚轮共同遵守该边界。切换时取消外部捕获、Slider gesture 和外部 Select/Tooltip 派生状态，保留背景绘制及应用的 disabled/open 值。TextArea 先依据顶层命中尝试滚动，未消费才交给窗口 participants；边界内子浮层继续使用原有 Escape/IME 优先级。

`modal_boundary` 使用真实 Input/TextArea/Search、Button、Switch/Checkbox/Radio、Slider、Select、Tooltip 和 Tabs，覆盖背景引用/选择拒绝、Tab 约束、动态 autofocus、嵌套/过期根、失效触发、IME Escape、滚轮回退、拖拽回调内切换、blur 回调转移/卸载及悬停 deadline 清理。无边界时恢复原有交互。

Debug 与 Release 完整 headless 构建成功；受影响输入、焦点、交互注册、模态 API/model/Token、既有控件及 backend boundary 测试各 **33/33** 通过，耗时分别 **7.37s / 4.73s**。日志：`out/modal-boundary-debug-regression.log`、`out/modal-boundary-release-regression.log`。源码格式检查493文件通过；OpenSpec doctor、61项 strict validation、`git diff --check` 通过。

本阶段只交付内部输入边界。实际可挂载对话框、遮罩、焦点恢复、Gallery 与真实窗口验收仍由后续任务完成，不计作已完成。
