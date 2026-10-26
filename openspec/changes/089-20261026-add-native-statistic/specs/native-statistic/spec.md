# Spec Delta

## Purpose

提供无需 Web 运行时的原生统计值展示，使用户能够组合标题、带前后缀的数值或富内容，在响应式更新、加载状态和有限布局中保持子控件身份、编辑草稿以及正确的裁剪与输入边界。

## ADDED Requirements

### Requirement: 稳定数值格式化

Statistic SHALL 接受有符号/无符号整数、有限浮点数和 UTF-8 String，默认值为零、小数分隔符为点、分组符为逗号。十进制输入 SHALL 保留符号和有效位，precision 对小数截断或补零；非十进制字符串 SHALL 原样显示。自定义 formatter SHALL 优先于内置格式化。非法非有限数值或 precision SHALL 在发布前拒绝，后续合法更新可恢复。

#### Scenario: 大整数和精确小数
- **WHEN** 输入 uint64 最大值或字符串 `-1234.123456789` 并设置 precision 为 8
- **THEN** 整数无 double 转换损失，小数显示为 `-1,234.12345678`

### Requirement: Retained 富内容与加载

Statistic SHALL 提供 typed title/value/prefix/suffix slots，普通属性、Theme、hidden、loading 和零尺寸恢复 SHALL 保留子内容身份、草稿与 refs。loading SHALL 使用既有 Skeleton 并阻止隐藏内容输入；隐藏根 SHALL 不保留 flow 或 gap。卸载 SHALL 清理本代资源与输入阻断。

#### Scenario: 加载和恢复
- **WHEN** 用户编辑富 value 后切换 loading、隐藏和零尺寸，再恢复
- **THEN** 已有编辑器及草稿保持，加载期间只显示标题和 Skeleton，隐藏内容不可聚焦或命中，最终卸载不遗留资源

### Requirement: 有限布局和场景裁剪

Statistic SHALL 产生真实逻辑场景，在有限宽、短高及零尺寸下约束标题和各值区域；glyph coverage、quad/effect 与 hit clip SHALL 遵守分配区域、祖先和 document clip，恢复时可再次显示。

#### Scenario: 狭小区域
- **WHEN** 富前缀、长值、后缀在短高或窄宽区域显示
- **THEN** 可见字形、装饰和输入均不越过各区域，零尺寸后恢复无需重挂载

### Requirement: 完整 Theme 合同

Statistic SHALL 支持 typed Theme override、独立 seed/algorithm、继承、相等性、identity、序列化及最小 dirty 订阅，最终派生字体 SHALL 至少为 0.5 logical dp，非法祖先更新 SHALL 在所有后代发布前原子拒绝。已有组件 token 与 identity 顺序 SHALL 保持。

#### Scenario: 非法祖先更新恢复
- **WHEN** 祖先 Theme 与后代 override 组合导致非法最终尺寸
- **THEN** 祖先及后代均保留原快照，后续合法更新可恢复，旧组件字段不变
