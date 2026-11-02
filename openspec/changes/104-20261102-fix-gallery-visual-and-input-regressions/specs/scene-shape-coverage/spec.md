## Purpose

保证控件形状、描边和路径在不同设备缩放和子像素位置具有完整且一致的可见覆盖，并在滚动、窗口层和透明背景组合时遵守真实裁剪与合成顺序。

## ADDED Requirements

### Requirement: Complete shape coverage
系统 SHALL 为矩形、圆角及圆形提交完整抗锯齿覆盖域，等宽描边的相对边不得因光栅范围不足产生系统性粗细差异。
#### Scenario: Fractional placement
- **WHEN** 控件位于不同物理像素相位或显示缩放
- **THEN** 四边及圆形极端位置的有效覆盖完整，形状尺寸和真实祖先硬裁剪保持

### Requirement: Shared selected border ownership
相邻选择项 SHALL 将选中描边及其 AA 覆盖合成一次，后项背景不得吞掉共享边。
#### Scenario: Vertical and horizontal options
- **WHEN** false/true/数字或横排、RTL选择项切换选中项
- **THEN** trailing共享边与其余选中边保持同一宽度与颜色覆盖

### Requirement: Clipped child fills
滚动容器 SHALL 对后代填充、文字及命中施加一致内容裁剪。
#### Scenario: Table row crosses header
- **WHEN** Table 的 Checkbox、InputNumber、Tag 随首行滚过表头
- **THEN** 行内容不穿入表头，窗口层开关后也不遗留旧填充

### Requirement: Continuous path joins
Tooltip箭头、圆角虚线及进度弧 SHALL 按真实几何连续合成，保留透明度且不产生接缝、异常短段或平切端帽。
#### Scenario: Tooltip and dashboard
- **WHEN** Slider/Rate提示切换方向或Dashboard显示零值和部分进度
- **THEN** 提示接缝无白线/加深，仪表盘保留设计开口并显示正确弧端

