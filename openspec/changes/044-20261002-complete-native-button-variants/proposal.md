# Proposal

## Why

用户要求补齐已有组件的原生桌面功能。Button 当前仅有 Default/Primary/Danger/Text、尺寸、禁用/加载和基本交互，目录明确列出的 dashed/link/ghost、预设色、图标位置与 wave 仍缺失；这些是桌面控件的可见功能，需要实际实现和验收。

## What Changes

- 增加 typed ButtonColor/ButtonVariant、Dashed/Link 类型、danger/ghost 与兼容旧类型的明确优先级；视觉值由 Theme/Component Token 派生。
- 增加 retained ButtonIcon/ButtonLoadingIcon slots、start/end 图标位置、icon-only、Default/Circle/Round/Square 形状与 block 布局；旧 ButtonContent 继续可用。
- 增加原生 ButtonRef focus/blur、mount autoFocus、可取消的 loading delay；新增 props 使用 Prop<T>。
- 使用共同 logical RoundedEffect 实现虚线圆角边框与有限时长 wave，遵守 motion/disabled/loading/window inactive 与销毁合同，不修改 GPU ABI。
- 补公开文档、Gallery 实例、native 支持状态以及共同测试、Windows/Linux 独立证据。

非目标：HTML href/target/htmlType、DOM refs、CSS/classNames/styles、兼容别名、React 节点检查和自动双汉字插空；原生 label 仍由 typed slot 中的 Text 明确声明。Space.Compact 的控件连接规则由后续 Space change 负责。没有新增平台宿主或 renderer 私有通道。

## Capabilities

### New Capabilities

- `button-native-variants`: 原生按钮的颜色/变体、retained 图标与加载、形状/焦点/布局和有限时长 wave。

### Modified Capabilities

无。`openspec list --specs` 当前没有主 specs，004 的 change 合同保留兼容；本 change 新增现有 API 未覆盖的行为。

## Impact

`include/ryn/button.hpp`、Theme tokens/派生/订阅/identity、Button component、HorizontalContentLayout、共同 retained content effects、Gallery 与 focused/headless/native tests。无新第三方依赖；预设色以锁定 Ant Design 6.6.5 为参考。主要风险是颜色优先级、透明 ghost 的真实虚线、loading 图标替换时的 retained identity、wave deadline 清理与回调销毁重入。
