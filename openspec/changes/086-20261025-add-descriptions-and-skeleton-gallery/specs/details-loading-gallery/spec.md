# Spec Delta

## Purpose

为 RynUI 使用者提供可直接从组件目录进入的详情编辑及加载预览流程，展示真实控件的内容修改、有限空间中的布局变化，以及隐藏加载期间的编辑状态保留和显式恢复。

## ADDED Requirements

### Requirement: 可编辑的详情布局

Gallery SHALL 提供可编辑详情，允许调整列数、跨列、显隐、标签方向、边框和尺寸，普通更新保留真实控件、草稿及公开 ref。

#### Scenario: 编辑与布局变化
- **WHEN** 使用者输入名称、选择角色、切换启用状态并改变布局
- **THEN** 保存反馈显示实际修改内容，原输入草稿与控件身份保留，隐藏字段可由外部控制恢复

### Requirement: 可恢复的加载预览

Gallery SHALL 提供可切换 loading、title、paragraph、avatar、round 的静态骨架和独立 avatar/button/input 形态；恢复操作始终位于被隐藏内容之外。

#### Scenario: 加载隐藏再恢复
- **WHEN** 使用者编辑正文、打开正文 Select 后启动加载，再恢复正文
- **THEN** 加载时正文不绘制或接受输入，旧 popup 和 preedit 清理；恢复保留普通草稿、组件、编辑器和 ref

### Requirement: 目录与有限视口

Descriptions 和 Skeleton 目录 SHALL 直接定位真实交互示例，在 Default、Dark、Compact 和宽窄有限视口中保持可操作、裁剪正确与焦点装饰可见。

#### Scenario: 从目录进入示例
- **WHEN** 使用者点击任一目标目录并用键盘和指针操作
- **THEN** 实际示例标题及控件可见，glyph、surface、hit 与焦点装饰受分配区和视口限制

### Requirement: 既有 Gallery 兼容

Gallery MUST 保留既有 stable IDs 的相对顺序、既有样例内容与非目标目录支持信息。

#### Scenario: 新示例追加
- **WHEN** 新示例进入目录和完整 Gallery
- **THEN** 旧样例仍按原顺序保留，旧布局、数值精度、内容所有权及 reset 行为不弱化
