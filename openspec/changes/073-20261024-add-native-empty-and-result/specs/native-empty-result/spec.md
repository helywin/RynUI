# Spec Delta

## Purpose

为 RynUI 应用提供可嵌入真实原生操作控件的空状态与结果展示，统一静态反馈的主题、约束布局和响应式更新合同，并在有限空间与普通属性更新中保留内容身份及用户已输入的数据。

## ADDED Requirements

### Requirement: Typed native presentation API

RynUI SHALL 提供 typed Props、typed slots 和 reactive Prop<T> 的 Empty/Result；Empty 支持默认/紧凑/无图示与 description、image/extra slots；Result 支持常规 success/info/warning/error 与 403/404/500、title/subtitle、icon/content/extra slots。常规状态 SHALL 复用 Status，LayoutStyle SHALL 仅控制外部布局。

#### Scenario: Reactive retained content
- **WHEN** 应用更新 description、title、subtitle、status 或主题，extra 内包含 Button/Input
- **THEN** 文本和图示更新，slot 不重新执行，Button 身份、Input editor 和已编辑值保持有效，操作继续可用

### Requirement: Constrained centered layout

组件 SHALL 在有限宽度内居中排列图示与标签，使文本正确换行并为 extra 保留间距；在短、窄或零约束下 SHALL 不产生负值或越界可见 glyph/icon，内容裁剪 SHALL 不裁掉原生控件自身装饰。

#### Scenario: Narrow and short content
- **WHEN** 使用长标签、真实 slot 内容并把分配区域收窄、缩短或缩为零
- **THEN** 实际 node/text/glyph/surface 几何保持有限，内容受分配区域裁剪，正常空间中的控件焦点装饰保留

### Requirement: Theme and static scope

组件 SHALL 支持默认、暗色、紧凑算法、继承、局部 override、equality、hash 与正确 dirty phase，并纳入五份诊断 golden；既有字段除 identity 外 SHALL 保持原值。首版 SHALL 明确为静态本地展示，内置图示使用维护中的 vector/Icon，不提供网络素材、服务、动态拓扑或 Web aliases。

#### Scenario: Theme updates and idle
- **WHEN** 更新组件颜色或尺寸 token 后再次同步到空闲
- **THEN** 颜色只触发材质相关工作、尺寸触发必要布局，slot 身份保留，组件不请求动画帧或服务
