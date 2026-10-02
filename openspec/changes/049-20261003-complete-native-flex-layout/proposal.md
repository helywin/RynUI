# Proposal

## Why

用户要求已有 partial 组件补齐原生桌面功能。源码审计确认 Flex 仅 bool wrap、四种 align 且默认 Start，缺少反向换行、字体基线和 RTL；grow/shrink/basis/order 已通过 LayoutStyle 提供，需要明确原生映射并验证，而非复制 CSS 字符串接口。

## What Changes

- 保留 `.wrap(bool/Prop<bool>)`，增加 typed NoWrap/Wrap/WrapReverse；双轴 gap、H/V、有限约束、空容器保持确定行为。
- 增加 Baseline 对齐与真实文字测量基线，组合容器/控件传递基线，无文字合成边缘基线；支持 align-self Baseline、RTL 和物理 Left/Right justify。等价上游值以 typed 原生含义映射。
- **BREAKING**：Flex 默认 align 从 Start 修正为 Stretch，对齐 Ant Design 6.6.5 默认行为；需要旧顶端对齐的调用显式 `.align(FlexAlign::Start)`，Gallery 与旧合同按实际意图迁移。
- 所有动态布局更新保留组件、scene、focus 身份；基线相关 align 变化更新 line 测量，纯对齐/RTL 保持 placement 局部更新。新增文档、Gallery、共同/Windows 证据。

## Capabilities

### New Capabilities

- `layout-containers`：补齐已有 005 的原生 Flex 容器合同；main specs 尚未建立，沿用已有 change 的 capability 名称。

### Modified Capabilities

无 main spec 修改；Space separator/Compact 与公共 Theme 配置在其各自后续 change 中实现。

## Impact

公开 Flex/LayoutStyle、Core LayoutEngine 的 line/baseline、Text 测量及共同内容布局、Gallery/tests/docs/catalog。无新增第三方依赖，Core/renderer 边界与 packed GPU ABI 保持。原生视觉、系统字体和 DPI 分 Windows/Linux 独立验收；当前机器只提供 Windows 实际证据。
