# Proposal

## Why

RynUI 已有 Button、Input、响应式属性及 change 013 提取的窗口服务与 `PressableBehavior`。继续逐个复制组件宿主会使当前 73 项目录的后续实现越来越慢且难以保持一致；现在用两个选中型控件验证这些共享边界。

## What Changes

- 使用 change 013 的 `WindowComponentServices`、窗口编辑设施和 `PressableBehavior`，让 Switch/Checkbox 只增加自身状态、键盘策略、布局及 Token 映射；Button/Input 的公开 API、视觉、输入与性能合同保持不变。
- 新增公开 `ryn::Switch` 与 `ryn::Checkbox`，使用 typed Props、reactive `Prop<T>`、必要的 typed slot、`LayoutStyle` 外部布局和 Ant Design 6.6.5 Component Token；覆盖 checked、disabled、focus-visible 与各自键盘语义。Switch 仅接受上游允许的 middle/small 两档，Checkbox 不提供 size Prop。
- 以 Button/Input 回归及 Switch/Checkbox 的共同交互、主题、场景、生命周期测试证明复用确实减少组件专属样板，且不会增加无关重组、全树上传或持续帧提交。
- 明确首批不包含 Checkbox Group、复杂表单集成、TextArea、Password/Search/InputNumber、通用公开控件基类或视觉 `Modifier`。

## Capabilities

### New Capabilities

- `shared-control-runtime`: 内部窗口服务、可组合按压行为、生命周期与最小失效的跨控件合同。
- `selection-controls`: Switch 与 Checkbox 的公开 API、状态、视觉、布局、输入和验收合同。

### Modified Capabilities

无；Button/Input 既有可观察合同保持不变，内部复用不修改它们的现有需求。

## Impact

- 影响 `src/component/` 的新控件实现、Gallery/测试接线和必要的共享服务扩展；change 013 的内部提取提交是本变更的前置基础，不在 011 重复实施；不把 SDL3、GPU 或内部 identity 暴露到公开头文件。
- 增加 Switch/Checkbox 的公开头文件、Theme Component Token 映射、测试和示例；第三方依赖模式不变，当前参考使用 change 012 升级后的 Ant Design 6.6.5 离线来源。
- 平台无关的 API、交互、渲染合同在一个受支持平台验证一次；真实窗口的系统输入、字体、GPU/shader 与 DPI 行为分别保留 Windows/Linux 验收，不能以一端替代另一端。
