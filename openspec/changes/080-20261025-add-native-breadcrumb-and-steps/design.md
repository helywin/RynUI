# Design

## Context

main 已包含可用的 Menu/Dropdown、Tabs、Segmented 和保留文字/图标。Menu 与 Segmented 采用固定 topology、稳定 key 和项内 reactive Prop，适合本次两类固定导航首版。Form/DatePicker 已通过独立 Astra 审查；TimePicker 和 Gallery 另行实施。

已读取本地锁定 Ant Design 6.6.5 breadcrumb/steps 文档：Breadcrumb 层级/分隔符，Steps current、四状态、orientation、medium/small 与 items 是首版基线。多种 Steps 类型和 router/Web 行为不进入此次实现。

## Goals / Non-Goals

**Goals:** 真实可点击/键盘使用的导航，保留富 slot 与属性更新，适配外部有限布局和现有输入边界；不以类型/静态占位代替运行组件。

**Non-Goals:** 不新增路由器、流程状态机或通用导航框架，不模拟 Web DOM；不扩展动态拓扑/自动 responsive、RTL 或动画，Gallery 单独接入。

## Decisions

1. API 使用固定 vector 的 typed items/slots，各项 title/disabled/visible 等为 Prop。固定 key 必须非空唯一，挂载时校验并保证失败清理。普通更新不执行 slot；显示项投影负责当前 Breadcrumb 和分隔符布局，隐藏内容保留身份但不占布局流。
2. Breadcrumb 使用自身导航 callback（含 key/index），最后可见项为当前非导航文字。支持默认 `/` 与 typed separator、富标题/图标；首版不合成 URL 或内置 Dropdown。
3. Steps current 使用索引及 controlled/default authority；大于等于固定项数量时视觉表示全部已完成，不做窄整型溢出转换或隐式回写。状态按原始索引推导，隐藏不重新编号 public index。项可覆盖状态，显示编号/check/error 或自定义 icon。只有 onChange 存在才开启步骤导航；roving Tab、方向键/Home/End 只移动焦点，Enter/Space/指针请求切换。子 slot 的原生激活不能被 wrapper 吞掉。
4. 复用现有 retained surface、Text/Icon 和 InteractionRegistry。尺寸先夹紧再居中，slot 后代用实际分配 clip；焦点装饰遵守有效祖先边界。eligibility 改变时刷新 HitTest，隐藏/输入边界变化沿现有清理路径处理，而非只改 paint flag。
5. 两组件独立 Theme 全链路，最终字体至少可被 Text 表示，所有最终派生度量有限且符合正/非负条件；关闭算法忽略局部 seed。保留主分支堆上 Theme binding/预校验及静态默认配置栈修复；五份 fresh golden 对照基线保留旧值和所有旧 IIFE 原文/顺序。
6. 使用 windows-msvc-headless / MSVC / Ninja Multi-Config 的 Debug/Release 完成平台通用 API/model/component/Theme 与受影响回归；真实 bounds/glyph/clip、输入和 retained 身份为证据，不要求另一 OS 重复相同逻辑，也不宣称 GPU/Linux/系统 IME 验收。

## Risks / Trade-offs

- [固定 topology 限制动态路由/工作流] → 文档明确重新挂载边界，现有项的显示、标题和状态继续响应；不额外引入 keyed diff 框架。
- [富 slot 与 wrapper 激活/裁剪冲突] → 验证真实子 Button/Input 激活、实际 glyph 和祖先 clip，焦点及销毁回调走原有服务。
- [Theme 结构增加导致栈或语义漂移] → 保留已验证的静态默认/heap helper，新增组件独立域，fresh golden/旧 IIFE 及组合回归验证。
