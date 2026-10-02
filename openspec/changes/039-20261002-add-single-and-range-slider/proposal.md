# Proposal

## Why

038 已完成当前后端框架的实现与 Windows 验收，组件可以通过共同 logical scene 合同继续开发。Slider 是现有 Data Entry 的明显缺口，本轮一次实现单值与范围选择，验证新组件可以复用共同输入、布局、主题与 retained surface，不再扩展 SDL 专用组件路径。

## What Changes

- 增加 typed `SliderProps` / `RangeSliderProps`、`SliderRange`、`SliderLimits` 和公开 Slider / RangeSlider；支持 controlled / uncontrolled、有限数值校验、step 对齐、动态 limits、disabled、keyboard、orientation、reverse、onChange / onChangeComplete。
- 实现轨道点击、mouse/touch capture 拖动、范围端点禁止跨越、独立 thumb Tab 焦点、方向键/Home/End/PageUp/PageDown、取消与窗口失焦/销毁恢复。
- 增加 Ant Design 6.6.5 Slider Component Token、Theme 继承/算法/覆盖与分阶段失效；通过共同 logical quads / rounded effects 保留 scene 身份和局部更新。
- 补充 Gallery 参考展示、组件合同测试、Recording scene 验证及独立 Windows / Linux native 验收。

非目标：Tooltip/marks/dots、整段范围拖动、多于两个 thumb、动态增删端点、公开 accessibility 平台桥接、动画插值、新后端。这些能力有独立依赖，本轮不以基础 Slider 的交付声称完整覆盖上游 API。

## Capabilities

### New Capabilities

- `slider-controls`：单值与范围 Slider 的数值、交互、主题与 retained scene 行为。

### Modified Capabilities

无；主 specs inventory 当前为空。

## Impact

影响公开 headers、component host、共同 keyboard keys、SDL key normalization、Theme snapshot/runtime、Gallery catalog 与示例、CMake tests。沿用依赖与 renderer ABI；错误数值、reentrant callback、drag cancellation 与 range focus 是主要风险，由负例及真实窗口证据验证。
