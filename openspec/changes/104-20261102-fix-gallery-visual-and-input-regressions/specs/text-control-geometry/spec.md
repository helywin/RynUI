## Purpose

为文字、图标、编辑光标与控件主体建立可验证的测量和放置行为，使中英混排、独立图标与富内容在控件布局变化时保持正确的对齐和空间分配。

## ADDED Requirements

### Requirement: Centered control content
控件 SHALL 根据实际文字和图标度量对齐内容，布局中心与绘制位置必须一致。
#### Scenario: Mixed controls
- **WHEN** Checkbox、Alert、Tree/TreeSelect、Avatar显示中英文字、图标或富内容
- **THEN** 对应行的垂直对齐正确，内容不因错误linebox/约束偏移或被遮挡

### Requirement: Editor and stepper geometry
编辑控件 SHALL 为OTP光标及InputNumber动作提供正确的中心与剩余空间分配。
#### Scenario: Empty OTP and numeric actions
- **WHEN** OTP空格聚焦或InputNumber改变宽度/值/门禁
- **THEN** caret按单格居中规则放置，上下箭头同轴且动作区位于控制区尾端

### Requirement: Content separation
Modal及富内容容器 SHALL 为说明、按钮与焦点装饰保留正确测量和间距。
#### Scenario: Loading confirmation
- **WHEN** 用户打开确认加载中对话框并聚焦内部切换按钮
- **THEN** 说明和按钮不重叠，焦点框完整且不遮挡文字

