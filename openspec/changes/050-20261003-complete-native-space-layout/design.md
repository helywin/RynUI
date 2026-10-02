# Design

## Context

见 proposal。Space 当前用 sequential FlexLayout，三种 align、bool vertical/wrap、Small/Middle/Large gap；没有 separator/Compact/Addon。049 已给 Text/Typography/Input、Box/Flex/HorizontalContent/InputContent 真实 baseline、retained placement 与按角 clipped logical effects。Button/Input size 当前有隐式 Middle，需要记录 explicit setter 来继承 Compact；Search 组合 Input+Button，RadioButton 已有 joined corner 与 seam 路径。

2026-10-03 锁定 [Space](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/space/index.tsx)、[Item](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/space/Item.tsx)、[Compact](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/space/Compact.tsx)、[compact item](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/style/compact-item.ts)、[Addon](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/space/Addon.tsx) 及 [Addon style](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/space/style/addon.ts)。上游 separator 是独立 flow item，可能随 wrap 换行；不强制 item/separator 成为不可拆分的一组。Space 无 responsive size API；horizontal 默认 Center、vertical CSS 默认 Stretch。

## Goals / Non-Goals

Goals：已有原生 Space 完整能力、共同 Compact 控件边界；不用 Component 重新执行解决 reactive 更新。

Non-Goals：新增 Select 等尚未实现组件、DOM/ref.nativeElement/React/classNames/styles、CSS 字符串/百分比解释器；公共 Theme 的全局 size/disabled/direction/locale 由后续收尾范围处理。Input 自身四视觉变体/TextArea/OTP 仍有独立后续 change，本次 Addon 实现其对应四类视觉。

## Decisions

1. SpaceAlign 追加 Auto/Baseline，Auto 由 orientation 决定 Center/Stretch。typed SpaceOrientation 与 bool vertical 最后 setter 优先；direction 采用现有 FlexDirection LTR/RTL。更新复用 049 FlexLayout、gap 和 baseline invalidation，不重新塑形方向更新的文字。保留 sequential policy 不接管 child grow/shrink/order。
2. SpaceProps 持有可选 SpaceSeparator typed slot，split alias 同一字段。ComponentBuildContext 的 scoped before-child hook 在第二个及后续直接项挂载前插入 passive separator branch；透明 Theme slot 共享 hook，嵌套子树不继承，调用自身的分隔时防止递归。这样逻辑布局、scene 与交互注册自然交错，不需要重排已经注册的焦点。separator 可含多个组件，由 wrapper 统一测量并继承 Space Theme；删除主项清理多余/首项前分隔，slot 生命周期及异常回滚由 ComponentHost 管理。无分隔时不增加 wrapper。
3. Compact 使用共同组件上下文注册 member callbacks（size/connection geometry/material priority），mount 时给 child 最近组，explicit size 留给自己。group shared state 与 owner lifetime ticket 断开 callbacks；nested group 按首尾条件相交外角。Search 的 Input/Button 作为现有组合参与，嵌套保留整体边界。优先于通过 token 修改 control_height，因为后者会混淆显式 child size 和组件 own Theme。
4. Compact layout 在当前 engine generation 测量/放置 retained child；用主题 border width 的负重叠而非普通负 LayoutStyle margin，block fill 可用宽度，方向只改坐标；空组和单项处理明确。unsupported passive内容继续布局，且不假装成可交互控件。
5. joined visual metadata 只在组件/共同 logical CPU scene 侧消费。Button 四角 fill/outline、focus/wave、dash；Input 固定 effect 层可在 compact 中拆为四 quadrant clips；RadioButton 合并已有 corners。主题 shadow 与状态继续由 own controls 产生。seam 按 hover、focus/active、normal、disabled 优先解决相邻边，保持稳定 paint/键盘声明顺序；使用额外有限 logical seam fragment，避免全局 z-index 或 GPU ABI 变更。
6. SpaceAddon 从锁定 Input/Addon token 派生逻辑尺寸、背景/边框/状态，四 variants 使用独立 typed API；抽出共享 InputStatus/InputVariant 类型供后续 Input 收尾复用。Addon content 获得 own foreground/typography，LayoutStyle 只管外部布局。绘制复用 RetainedSurfaceService 和 corner helpers。
7. Unit/HEADLESS 契约在本机 Windows/MSVC `windows-msvc-headless` Debug/Release 完成一次。Windows `windows-msvc` D/R Gallery frame、default-stack smoke、多尺度真窗口验证连接形状、键盘/指针/编辑、resize/DPI/idle/销毁。Linux 真实 GPU/系统字体/window/input 另列，不重复平台通用合同。

## Risks / Trade-offs

- [默认对齐影响旧样例] → 显式 Start 迁移需要旧意图的代码，单独记录 BREAKING。
- [组合或嵌套边界漏失] → typed nearest contexts、nested/mixed/status/三尺寸/H/V/RTL/零约束合同，删除焦点项后重算外角。
- [跨 renderer/额外持续帧] → 仅 bounded logical effects、统一 scene上传；真实闲置与资源归零验收。
- [自定义 Theme 误覆盖尺寸] → explicit Props 优先和每控件 own tokens，不用全局 token hacks。
- [历史验收与新 binary 混淆] → 每次记录实际 EXE/PNG hashes，保留先前证据为历史记录。
