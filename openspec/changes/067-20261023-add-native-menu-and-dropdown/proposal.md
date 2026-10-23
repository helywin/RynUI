# Proposal

## Why

当前有 Select、Tooltip 与 Modal 的 retained 浮层基础，但导航菜单和命令下拉仍没有原生组件。需要可直接承载桌面导航与操作的 Menu / Dropdown，并保持键盘、受控状态与模态归属一致。

## What Changes

- 增加稳定 key 的 Menu、响应式文本/禁用/危险状态、单选或多选请求、分组/分隔线与 Inline 子菜单展开请求。
- 增加 typed trigger slot 的 Dropdown，复用 Menu，支持点击、右键定位、键盘、外部点击/Escape、焦点恢复、窗口内翻转与裁剪。
- 独立 Menu/Dropdown Token 消费 Default/Dark/Compact Theme；沿用 WindowComponentServices 与 logical scene，保持 CPU scene v3 / GPU ABI v2。
- 首版不实现 horizontal overflow、vertical flyout 子菜单、hover 延迟、虚拟化、Web DOM/CSS 或自定义 popup renderer；Dropdown 子菜单在同一菜单中展开。

## Capabilities

### New Capabilities

- `native-menu`: retained 导航菜单与受控选择/展开。
- `native-dropdown`: 窗口命令菜单、触发器、定位和生命周期。

### Modified Capabilities

无。

## Impact

增加公开头、内部 component、局部 Theme/service/CMake 接线、聚焦测试和 docs/menu-dropdown.md。复用 Select 的行资源/裁剪、Tooltip 的定位、Modal 的逻辑归属；Popconfirm 分支已拥有共同 child-layer 排序修正，本变更不重复实现。Gallery/README/全局支持清单由父任务集成。风险集中在回调卸载、受控拒绝和初始隐藏 Modal，必须以实际 MSVC headless 测试验证；本变更不声明原生 GPU、Linux 或 IME 验收。
