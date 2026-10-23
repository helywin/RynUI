# Spec Delta

## Purpose

为原生业务界面提供可以组合标题、补充操作、封面、正文与底部动作的内容卡片，使应用能够复用一致的布局与主题，同时在属性和加载状态变化时保留内部控件及其用户数据。

## ADDED Requirements

### Requirement: Typed content regions

RynUI SHALL 提供 typed Card Props 和 body/title/extra/cover/actions slots，支持 reactive 标题文本、默认/小号、outlined/borderless、hoverable/loading；LayoutStyle 仅控制外部布局，视觉来自 Theme/Component Token。

#### Scenario: Compose business content
- **WHEN** 应用挂载含标题、补充 Button、正文 Input 与底部动作的 Card
- **THEN** 各区域按 header、cover、body、actions 顺序布局，子控件可正常聚焦和操作，Card 本身不增加 Tab 停靠点

#### Scenario: Small or narrow container
- **WHEN** Card 使用小号或有限宽度，标题与 extra 同时存在
- **THEN** 使用对应尺寸 Token，header 至少达到主题高度，子区域尺寸非负并受可用宽度约束，正文高度由实际内容决定

### Requirement: Retained loading and hover

Card SHALL 在普通属性更新中保留所有用户 slots 的组件 identity 与状态。loading 时显示静态占位并隐藏正文，正文不可命中和获得焦点；离开 loading 后恢复相同正文。hoverable 仅改变视觉，不夺取子控件的输入或动作。

#### Scenario: Loading retains editable content
- **WHEN** 用户编辑正文 Input 后，应用切换 loading 再恢复
- **THEN** loading 时旧正文焦点及输入会话清理，恢复后 Input identity 和已输入值不变，slot content 不重新执行

#### Scenario: Nested action under pointer
- **WHEN** 指针移入 hoverable Card 的内部 Button 并点击
- **THEN** Card 显示悬停视觉，Button 回调正常执行一次，Card 不拦截其命中

### Requirement: Local themed presentation

Card SHALL 遵循锁定 Ant Design 6.6.5 的颜色、header/body 尺寸、字体、圆角与阴影配方，支持继承/reset、Component Token 显式覆盖和独立算法；颜色、尺寸、字体与效果更新按其失效域处理。

#### Scenario: Color-only theme update
- **WHEN** 应用只更新 Card 的背景或边线颜色
- **THEN** 仅修改 Card/相关文字材质，不重新执行 slots，不测量无关组件，不改变其他组件 Token

#### Scenario: Size and effect overrides
- **WHEN** 应用更新 body padding、header 字体或 hover shadow
- **THEN** 对应几何、文字或效果局部更新，snapshot identity/hash/序列化包含这些值，非法非有限或负尺寸被原子拒绝

### Requirement: Native lifecycle and truthful coverage

Card SHALL 通过共同 logical scene 与窗口资源绘制，销毁时清理场景、订阅和 interaction；平台通用、Windows 与 Linux 原生验收独立记录。首版不将 Grid/Meta/内置 Tabs/异步图片或 Web API 计为已实现。

#### Scenario: Disposal and idle
- **WHEN** Card 完成有限交互后静止，随后被销毁
- **THEN** 静止时不持续请求帧，销毁后不存在旧节点、命中、订阅或效果资源

#### Scenario: Platform evidence
- **WHEN** 仅在 Windows 实际执行窗口、GPU、字体与缩放验收
- **THEN** 只完成 Windows 项，Linux 和人工系统输入法缺少实际证据的项目保持待验收
