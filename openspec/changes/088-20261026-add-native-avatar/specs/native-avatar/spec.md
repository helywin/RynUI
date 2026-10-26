# Spec Delta

## Purpose

提供以 Ant Design 6.6.5 为设计基线的原生文字与图标头像，以及固定 key、重叠布局和真实溢出内容展示的头像组。属性更新保持 retained 身份，组件在受限尺寸、显隐、主题更新与卸载后保持可恢复和资源完整。

## ADDED Requirements

### Requirement: Native Avatar

Avatar SHALL 提供 typed Props、reactive 属性、circle/square、三种预设尺寸和正数自定义尺寸、文字 gap、typed retained icon/content。icon 优先于文字；显隐不得重建子内容，隐藏不得占 flow/gap。文字 SHALL 使用真实字体测量缩小适配，发布字体不得小于 .5 logical dp。外部布局由 LayoutStyle 控制，视觉由 Theme 控制。

#### Scenario: 约束与恢复
- **WHEN** 文字、尺寸、显隐或有限宽/短高约束变化并经历零尺寸后恢复
- **THEN** 文字重新适配且真实 glyph/quad/hit 均受内容区域裁剪，富内容身份、草稿和 ref 保留，装饰能够在祖先允许的区域外扩

### Requirement: Retained AvatarGroup overflow

AvatarGroup SHALL 接受固定、唯一非空 key 的文字/原生图标条目，保留每个展示位置的 retained 身份，支持条目显隐、统一尺寸/形状和重叠。现代 typed max.count SHALL 限制内联原始头像数量；只有可见条目数量超过 count 才显示准确 +N，默认 hover 的原生 Popover SHALL 展示实际隐藏头像。首版 Group 不接受任意富 slot，避免重复执行带 ref 的用户内容。

#### Scenario: 动态溢出
- **WHEN** max.count、条目显隐或 Group 显隐变化
- **THEN** 内联与 Popover 的可见条目及 +N 一致，隐藏条目不参与 flow/hit；未溢出时无 overflow 触发器，固定条目更新不重新执行构建

### Requirement: Theme 与生命周期边界

Avatar Theme SHALL 支持继承、独立 seed 算法、override、比较、identity、序列化与按域 dirty；非法最终派生字体/尺寸 SHALL 在发布前拒绝，祖先更新对子孙执行原子 preflight。组件 SHALL 复用 logical scene v3 与现有 Popover/Input 边界，完整 generation 清理 owned blocks 和 scene 资源，无 idle deadline。

#### Scenario: 非法更新与卸载
- **WHEN** 非法主题更新、Modal/Spin 输入 gate、同步卸载或 ref 重绑发生
- **THEN** 保持旧有效主题且可恢复；输入边界正确，旧 generation 不再回调，卸载不残留 block、glyph、quad 或 deadline
