# Proposal

## Why

原生状态展示缺少可关闭、可选择的标签及计数徽标。补充 Tag 与 Badge，直接复用已有文字、图标、surface 和交互服务。

## What Changes

- Tag：typed content/icon slots、预设/状态/自定义颜色、bordered、disabled、受控 visible/onClose、CheckableTag。
- Badge：整数 count、overflowCount、showZero、dot、status/text、offset、大小，以及 retained child/独立使用。
- 主题组件 Token、针对性 headless 测试和组件示例。
- 首版不包含 href、CheckableTagGroup、Ribbon、滚动数字、进退场或 processing 波纹动画；Gallery 与综合目录由父任务集成。

## Capabilities

### New Capabilities

- `native-tag-and-badge`：原生标签交互及徽标状态展示。

### Modified Capabilities

无。

## Impact

新增公开头文件、组件 host、主题解析与订阅；局部修改构建和窗口服务注册。无依赖、CPU scene v3 或 GPU ABI v2 变更。风险集中于浮出 child 的徽标布局、关闭焦点清理与属性更新身份，使用平台通用测试验证。
