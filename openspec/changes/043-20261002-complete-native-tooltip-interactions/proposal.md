# Proposal

## Why

040 的 Tooltip 只有 String title 与 hover/focus，尚不能给已有组件提供富标题、点击和右键提示。用户要求补齐已有组件的原生桌面能力，需要完成这些合同，并消除细条 quad 箭头在 GPU 上的视觉近似。

## What Changes

- 新增 retained typed `TooltipTitle` slot，兼容现有 String title，提供 reactive 可用性控制与语义文字继承。
- 新增 Click、ContextMenu 和 typed 组合 trigger；共用非消耗 pointer 观察实现窗口空白处关闭，保留子控件 click/capture/focus。
- 新增 reactive arrow pointAtCenter，角位置遵循锁定 Ant Design 6.6.5 的边缘/中心关系；右键锚点跟随指针。
- 将箭头改为私有向量 glyph，复用现有 FreeType/R8 atlas/logical scene/renderer 上传合同。
- 补 API 文档、Gallery 样例、headless 合同与真实 Windows GPU/DPI 验收。

## Capabilities

### New Capabilities

- `tooltip-content-and-actions`：富标题寿命、组合指针触发、外部关闭、箭头对齐与共用矢量呈现。

### Modified Capabilities

无。主 specs inventory 为空；本 change 扩展 040 delta 的共用浮层，保留原有 API 与生命周期。

## Impact

涉及 Tooltip/public typed slots、PointerRouter 的窗口观察、WindowComponentServices、私有图标 outline 生成器、Gallery 和 tests。无新增依赖、GPU ABI 或组件私有上传；Theme/token 控制视觉，LayoutStyle 仍仅用于外部布局。

不移植 DOM/CSS/ref 节点、Web 别名、React renderer、Popover 交互容器。风险主要是 pointer 回调销毁/失效、受控状态回写、slot 布局更新和矢量 glyph 方向/裁剪。通过可独立提交的合同及真实窗口阶段验证，Linux 原生项独立保留。依用户“写完 change 就开始改代码”授权，规划提交后立即 apply。
