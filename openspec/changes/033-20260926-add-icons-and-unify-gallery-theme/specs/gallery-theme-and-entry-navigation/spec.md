## ADDED Requirements

### Requirement: Scrollbar pointer state feedback

滚动条 SHALL 在轨道或滑块悬浮、按下与拖动时显示来自当前主题的可辨认颜色变化。

#### Scenario: Hover and drag scrollbar
- **WHEN** 指针进入滑块、按下并拖出轨道后松开
- **THEN** 显示悬浮、拖动和最终普通/悬浮状态，拖动捕获保持；取消或失焦后不残留按下颜色

#### Scenario: Track click and dark theme
- **WHEN** 用户在亮色或暗色下悬浮并点击轨道
- **THEN** 轨道提供状态反馈且翻页仍正确，状态变化不导致正文重排版

### Requirement: Legible text navigation

导航 SHALL 使用无边框文字按钮，在普通、选中、悬浮、按下及键盘焦点状态保持可读，主题样式来自 Theme。

#### Scenario: Hover over a selected or normal row
- **WHEN** 指针进入亮色或暗色主题的任意导航行
- **THEN** 行背景与文字保持可辨认，不出现实心同色边框层遮住文字

### Requirement: Every component entry navigates

组件子项 SHALL 是可点击、可键盘激活的导航入口，以稳定组件 identity 跳转到正文对应卡片，并显示选中反馈。

#### Scenario: Select a component entry
- **WHEN** 用户选择任意组件子项
- **THEN** 对应卡片进入正文 viewport，左栏不会被正文滚动带走，窄窗口也能从目录跳回该卡片

#### Scenario: Navigate to a filtered entry
- **WHEN** 当前筛选隐藏了用户选择的组件
- **THEN** 恢复包含该组件的可见内容后定位，不能静默不响应或跳到旧几何位置

### Requirement: Unified global theme changes

所有全局主题入口 SHALL 同步窗口背景、正文、导航、图标和控件颜色；固定顶栏提供亮暗切换，切换保留组件身份和交互状态。

#### Scenario: Manual light to dark and back
- **WHEN** 用户通过顶栏或示例按钮切换主题
- **THEN** clear color 与根 Theme 一致，字体和图标可读，返回亮色时无残留暗色块，当前滚动与编辑状态保持
