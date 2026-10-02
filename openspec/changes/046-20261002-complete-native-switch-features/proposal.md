# Proposal

## Why

用户要求补齐已有组件的原生桌面功能。现有 Switch 只有固定宽度轨道与布尔交互，缺少 Ant Design 6.6.5 的状态内容、焦点引用、激活回调、RTL 和完整手柄反馈，不能仅将目录状态改为 implemented。

## What Changes

- 增加 retained checked/unchecked typed slots，支持原生文字、图标与被动布局；按两种内容的最大宽度确定稳定轨道宽度，状态切换不重跑 slot。
- 增加 owner-thread SwitchRef、mount-only autoFocus、onClick(bool)、reactive direction 与 wave。保留现有 checked/defaultChecked、Space 语义和 loading 保持有效焦点的合同。
- 补齐内容边距、手柄阴影、按压伸展、主题 opacityLoading 与有限激活 wave；内部内容裁剪复用 logical scene，不引入私有 renderer/SDL 上传。
- 更新 Gallery、API 文档、目录证据和共同测试，分别记录 Windows 与 Linux 原生窗口/GPU/DPI 验收。

## Capabilities

### New Capabilities

- `switch-presentation`: 原生 Switch 状态内容、焦点引用、回调、方向、主题及有限动画的完整合同。主 specs 当前为空；现有 011 的 selection-controls 保持兼容。

### Modified Capabilities

无。

## Impact

公开 switch.hpp、SelectionComponentHost、Theme Switch token、共同布局/文字裁剪、Gallery 和测试。旧 Switch(props) 调用保持兼容，Checkbox/Radio 现有行为不得回退。主要风险是内容预算与裁剪、回调销毁、ref 重复绑定、动画取消和主题 phased invalidation；均由合同测试覆盖。

非目标：DOM/HTML/CSS、React ref、Web value/defaultValue 别名及任意可交互子控件。Windows 使用 MSVC/Ninja Multi-Config；Linux 原生证据需实际 Linux 机器，缺少证据时保持待办。
