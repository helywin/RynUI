# Proposal

## Why

业务页面已有菜单、标签页和表单，仍缺少显示层级位置与分步流程的常用导航。补齐原生 Breadcrumb 和 Steps 的真实可用首版，便于组合详情页、设置流程及录入向导。

## What Changes

- Breadcrumb：固定稳定 key 的层级项、响应式标题/禁用/可见状态、typed 富标题/图标与 separator slots，真实祖先项激活和当前项表现。
- Steps：固定步骤项、受控/默认 current、Wait/Process/Finish/Error 状态、标题/副标题/说明/图标 slots，默认/Small 两尺寸和横纵排列，真实指针/键盘切换请求。
- 保留 item/slot 身份，支持隐藏项布局清理、实际有限 glyph/surface/hit clip、焦点和 Modal/Spin 边界；接入完整 Theme 链路及可编译 API 文档。

首版不包含动态拓扑替换、Web router/href/params、Breadcrumb 内置 dropdown、Steps dot/inline/navigation/panel/percent/maxCount、自动 responsive/RTL、动画或辅助技术桥接。普通属性继续响应，调用方需要改变拓扑时重新挂载。Gallery 与原生平台验收独立推进。

## Capabilities

### New Capabilities

- `native-breadcrumb`: 原生保留层级导航、当前项和 separator 表现。
- `native-steps`: 原生保留步骤状态、横纵布局与受控导航。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

涉及公开 Props/items/slots headers、host/model、Theme/token/runtime、窗口服务/CMake、focused tests 与 docs/breadcrumb.md、docs/steps.md。参考已读取的本地锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 breadcrumb/steps 文档及样式；复用既有文字/图标、交互和 retained scene，不新增依赖或改动 renderer/GPU ABI。
