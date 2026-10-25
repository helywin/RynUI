# Spec Delta

## Purpose

为 RynUI 提供具有原生布局、主题与输入生命周期的时间轴，让事件标题、正文和节点既能展示文本，也能容纳真实的按钮、编辑器和选择控件，并在响应式更新中保留用户正在编辑的内容。

## ADDED Requirements

### Requirement: Modern vertical timeline

Timeline SHALL 提供 typed Props、固定 keyed items、Start/End/Alternate、placement override、reverse、title/content 文本和 typed Title/Content/Icon slots、loading、tone 与 visibility。无效 key/enum MUST 拒绝；不得公开 deprecated 或不可用入口。

#### Scenario: Placement and reverse
- **WHEN** Alternate items 包含 placement override 并反序
- **THEN** placement 保留源索引含义，显示和真实 Tab 顺序反转，末尾无残留轨迹

#### Scenario: Reactive text and visibility
- **WHEN** 修改文本、mode、reverse、tone、loading、显隐或 Theme
- **THEN** slot 只挂载一次，编辑器、草稿与 refs 保留；空/全隐藏不占布局流和 gap

### Requirement: Actual visual regions

Timeline SHALL 呈现真实 glyph、节点与轨迹；title/content/icon 各自裁剪于分配区。有限、极窄、短/零高及恢复 MUST 保持 surface/glyph/hit 裁剪；focus 外扩装饰可见且服从外部 hardclip。

#### Scenario: Narrow and short recovery
- **WHEN** 有真实 Input/Button/Select 的富 slots 经极窄、零高后恢复
- **THEN** 无可见 ink 或 hit 越区，不缩小真实控件规避溢出，身份不变且可继续操作

### Requirement: Safe input boundaries

隐藏及祖先 Modal/Spin suppression SHALL 清理 popup、preedit、focus、capture；恢复不得复活被强制关闭的受控拒绝 popup。普通 controlled close SHALL 尊重 authority。同步回调卸载或重绑 MUST 安全，teardown 只清理 owned input blocks。

#### Scenario: Callback unmount
- **WHEN** 隐藏触发 popup close 或 blur 回调同步卸载时间轴
- **THEN** 不访问旧 generation，无 owned block 泄漏，foreign owner 保持原状态

### Requirement: Complete independent theme

Timeline SHALL 支持独立 typed colors/metrics、全链继承与算法、hash/equality/diagnostic、精确 dirty 与 append-only identity。最终不可渲染字体或非法组合 metric MUST 整树原子拒绝并可恢复；旧 token 值和 identities 不变。

#### Scenario: Descendant rejection and recovery
- **WHEN** 父配置和后代 override 的最终组合非法
- **THEN** 父及后代 snapshot 均保持原值，合法更新后统一恢复

### Requirement: Retained portable scene

Timeline SHALL 使用现有 logical scene、资源和 dirty 调度，Core 不依赖 renderer/SDL/OS，无新依赖；idle 和静态 loading MUST 不持续请求帧。

#### Scenario: Idle loading
- **WHEN** loading 节点稳定且无属性变化
- **THEN** 不重复发布 geometry/material，不产生下一帧 deadline
