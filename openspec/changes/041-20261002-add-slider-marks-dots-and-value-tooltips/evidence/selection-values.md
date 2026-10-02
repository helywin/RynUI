# Slider 标记数值合同

2026-10-02，Windows 11 / MSVC，Ninja Multi-Config，windows-msvc-headless-debug/release。

阶段 2 已实现 typed marks/marksOnly/dots/included/hint 配置、完整配置验证和 step/marks 共享候选算法。标签、dot 与 Tooltip 绘制在阶段 3 继续；不把本阶段的 API 声明视为已交付所有视觉功能。

focused rynui.portable.slider_component 在 Debug/Release 均通过，覆盖旧步长/范围输入合同及新增：无序 marks 排序、非网格 mark、最近点/等距、marks-only 边界、方向键前后相邻点、Page 边界、非整除 maximum 返回最终 grid、dots 集合去重、4096 点边界/超限、NaN/重复/越界标记、dense 未启用 dots 不枚举、reactive marks/limits 失败保留 state、marks 重归一化不触发 onChange。

公开选项：`SliderMark{double value,String label}`、`marks(Prop<SliderMarks>)`、`marksOnly(Prop<bool>)`、`dots(Prop<bool>)`、`included(Prop<bool>)`、`hint(Prop<SliderHintOptions>)`、`hintFormatter(std::function<String(double)>)`。marksOnly 表达只选 marks/min/max，保留旧 `SliderLimits{min,max,step}` 聚合和 step 正数合同；formatter 与 onChange 一样为挂载时 callback，mode/placement 为可比较 reactive options。
