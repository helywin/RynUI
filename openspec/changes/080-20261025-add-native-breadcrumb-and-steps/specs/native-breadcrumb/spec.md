# Spec Delta

## Purpose

为原生业务页面提供可操作的层级位置展示，使用户能够识别当前位置并返回祖先层级。导航项采用稳定身份和保留内容，允许响应式更新标题、禁用及可见状态，遵守现有主题、焦点、输入边界与有限布局合同。

## ADDED Requirements

### Requirement: 层级表现与真实导航

Breadcrumb SHALL 展示有稳定唯一 key 的固定层级项、typed 富标题/图标与 separator；最后一个可见项表示当前位置且不作为祖先导航入口。启用的祖先项 SHALL 通过指针或键盘发出准确 key/index 激活请求，不自动打开 URL。

#### Scenario: 返回祖先
- **WHEN** 用户用指针或键盘激活祖先项，随后禁用该项或隐藏末级项
- **THEN** 首次发送准确导航请求，禁用后不发送；当前项随最后可见项更新且无多余首尾分隔符

### Requirement: 保留布局与输入资格

标题、禁用、可见及 Theme 更新 SHALL 保留已挂载 item/slot 身份。隐藏项和全空 Breadcrumb SHALL 不保留普通布局占位；实际 glyph/表面/命中 SHALL 遵守分配范围及祖先 clip，隐藏或输入边界变化清理焦点/捕获。

#### Scenario: 隐藏和恢复
- **WHEN** 在带 gap 的布局中隐藏全部项，再恢复并切换 Theme 或限制宽高
- **THEN** 相邻内容正确重排，恢复后保持原身份，重新启用的可见项可实际点击，裁掉的内容不能命中

### Requirement: 独立 Theme

Breadcrumb SHALL 支持独立 Theme 派生、继承与 typed override，最终字号/尺寸在发布前校验；未启用的局部 seed 不得丢失父级覆盖值。外部布局由 LayoutStyle 控制。

#### Scenario: 继承和非法派生
- **WHEN** 子 Theme 的组件算法关闭但有局部 seed，或开启后派生非法字号/尺寸
- **THEN** 前者保持父级覆盖，后者在发布前拒绝并保持原状态；颜色更新不重建 slot
