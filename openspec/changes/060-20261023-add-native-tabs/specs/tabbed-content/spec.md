# Spec Delta

## Purpose

为原生业务页面提供可键盘操作的分区导航，让应用通过稳定的字符串标识切换内容，并在标签切换期间保留各面板内表单草稿、原生控件身份和业务订阅，避免重建整个页面。

## ADDED Requirements

### Requirement: Typed keyed navigation

RynUI SHALL 提供Tabs Props、固定集合的TabItem与typed TabContent，每项有非空唯一String key、响应式label/disabled；支持activeKey/defaultActiveKey、onChange/onTabClick。重复/空key和冲突控制属性MUST在挂载内容前拒绝。

#### Scenario: Initial and explicit keys
- **WHEN** 没有指定activeKey/defaultActiveKey
- **THEN** 选择第一个可用标签；空集合或全禁用时没有初始面板

#### Scenario: Controlled request refused
- **WHEN** 用户激活不同标签而父级不回写受控activeKey
- **THEN** onChange请求对应key，仍显示原面板，焦点可以移动到请求的标签，组件不重建

#### Scenario: Unknown or disabled explicit key
- **WHEN** 显式activeKey为未知或禁用标签
- **THEN** 保留该key；未知值显示空面板，已存在的禁用项仍可由显式值显示，用户导航不能激活禁用项

### Requirement: Accessible horizontal tab strip

Tabs SHALL 提供line/card、Small/Middle/Large、Top/Bottom及centered。pointer与Left/Right/Home/End/Enter/Space支持可用标签激活，标签组只有一个Tab入口，面板内控件维持自己的焦点顺序。

#### Scenario: Keyboard and disabled items
- **WHEN** 标签获得焦点后使用方向键或Home/End
- **THEN** 跳过禁用项，移动焦点并请求激活；Enter/Space请求当前标签，重复激活只通知onTabClick而不重复onChange

#### Scenario: Overflow and resizing
- **WHEN** 标签总宽度超过可用宽度
- **THEN** 提供非Tab停靠的前后滚动控件和滚轮导航，键盘激活项自动显露，文字/背景/命中限制在标签视口；窄宽度及resize不产生负尺寸或不可恢复的滚动偏移

### Requirement: Retained panels and clean deactivation

Tabs SHALL 将所有TabContent一次预建，只显示选中面板；隐藏面板不可命中/聚焦，其输入会话和窗口层清理。切回时保留用户数据、editor/组件identity与订阅，不重新执行TabContent。

#### Scenario: Draft and nested popup restoration
- **WHEN** 用户编辑面板Input、打开Select后切到另一个标签，再切回
- **THEN** 旧输入/弹层关闭，草稿和原控件保留，恢复不自动抢焦点，其他页面不重新执行

#### Scenario: Callback removes owner
- **WHEN** onChange/onTabClick或切换导致的blur回调卸载Tabs
- **THEN** 旧指针捕获、焦点、节点、命中与场景安全失效，不访问已销毁状态

### Requirement: Local themed presentation

Tabs SHALL 依据锁定Ant Design 6.6.5提供Component Token及继承/reset/独立算法/显式覆盖；颜色、尺寸、文字和效果按失效域更新，既有组件Token值保持。LayoutStyle仅管理外部布局。

#### Scenario: Color-only update
- **WHEN** 仅修改标签颜色、指示条或卡片背景
- **THEN** 局部更新材质，不重测无关组件、不重新执行面板内容

#### Scenario: Geometry and typography update
- **WHEN** 修改字号、gutter或尺寸
- **THEN** 局部重新布局并约束溢出，键盘和命中继续对应正确标签，非法尺寸被原子拒绝

### Requirement: Explicit native coverage

Tabs SHALL 使用共同logical scene并在idle/销毁时停止无效更新、清理资源；平台通用、Windows和Linux验收独立记录。首版不将动态集合、vertical/RTL、富标签/额外栏、overflow popup、动画或lazy/destroyOnHidden计为已实现。

#### Scenario: Idle and platform evidence
- **WHEN** Tabs静止或销毁，且只有Windows实际窗口证据
- **THEN** 静止不持续请求帧，销毁无资源遗留；只完成Windows项，Linux及人工系统IME缺少实际证据的项目保持待验收
