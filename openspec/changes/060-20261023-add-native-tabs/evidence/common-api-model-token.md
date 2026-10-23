# Tabs API、模型与Token

2026-10-23，实际Windows/MSVC x64，`windows-msvc-headless` Debug/Release。

新增TabsProps、TabItem和typed TabContent；集合固定，label/disabled为Prop，提供String active/default key、类型/方位/尺寸/centered及回调。此阶段提供公开声明，尚未接入可挂载Tabs，Gallery状态仍为planned。

独立TabsModel验证初始首个enabled、空/全禁用、显式未知/空/禁用值、controlled拒绝与回写、重复请求、disabled动态变化、首尾/循环导航、非法/重复key及控制冲突。String key按完整字节区分，包括大小写和内嵌NUL，不隐式改写既有选择。

Tabs Token包含11个颜色、17个尺寸、三尺寸字号/行高与字重、3个focus效果尺寸，支持四组dirty identity、继承/reset/独立seed算法、显式override、hash与诊断序列化。验证默认line padding8/12/16、card高度32/40/48、gutter32/2、原子拒绝非法/溢出尺寸与字重。五份golden按值对照d326573，确认此前所有组件含Select/Card均未改变，仅新增Tabs和整体identity。

```text
Debug/Release headless builds: passed
affected CTest: 24/24 each (1.95s / 1.53s)
filter: tabs_|card_|theme_|select_|public_api|backend_boundaries
5 golden preservation checks: passed
clang-format 22.1.3: 482 owned files, 0 failures
doctor healthy; strict 60/60; git diff --check passed
```

真实导航/保留面板/overflow、Gallery和原生平台仍在后续任务，未勾选。
