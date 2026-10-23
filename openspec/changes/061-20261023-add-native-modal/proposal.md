# Proposal

## Why

已有 Input、Select、Card 和 Tabs 可以组合业务页面，但缺少原生模态确认/编辑对话框。用户要求优先补齐常用组件，本变更接续060，提供能保留表单草稿、隔离背景交互并正确关闭/恢复焦点的 Modal 首版。

## What Changes

- 新增 typed reactive ModalProps、ModalContent、可选标题/页脚 slots，提供受控 open、标题/默认或自定义页脚、OK/Cancel/关闭按钮、遮罩/Escape 取消、confirmLoading、宽度/居中和保留内容。
- 复用窗口层、Constraints、Button/Input、既有圆角/阴影效果；对话框有限适配视口，正文溢出可滚动，不创建 SDL 类型或新GPU ABI。
- 新增内部模态交互边界：只允许最上层 Modal 的逻辑子树接受指针、键盘、输入和滚轮；Tab 环绕、初始焦点/关闭恢复、嵌套 Modal 与子 Select/Tooltip 窗口层有明确生命周期。
- 新增 Modal Token、局部失效/继承/reset/component algorithm/golden、真实控件测试、Gallery 与独立 Windows/Linux 原生证据。
- 首版不提供动态销毁/重建内容、动画/静态服务式 confirm/Promise 自动关闭、拖动/resize、mask blur、vertical/RTL、断点宽度、可访问性平台桥接或 Web/DOM/CSS API；目录保持 partial 并列出缺口。

## Capabilities

### New Capabilities

- `native-modal`：原生受控模态对话框、交互隔离/焦点恢复、保留内容、窗口布局/滚动与主题。

### Modified Capabilities

无已归档的正式 capability 需要修改；现有输入、非模态窗口层和非模态页面合同继续保持。

## Impact

涉及 `include/ryn/modal.hpp`、共享交互注册/焦点与窗口组件服务、Modal host/model、Input/Select/Tooltip/Tabs 的模态输入边界协调、主题与序列化/golden、测试和 Gallery。无新增依赖，不改 renderer GPU ABI、SDL 边界或外部组件自身 disabled 值。平台通用验证使用 Windows/MSVC 的 headless preset 一次完成；Windows D3D12/DXIL 和 Linux native Vulkan/SPIR-V 单独验收，未执行的平台不勾选。每个可验证阶段独立英文提交、立即 push 并核对远端 SHA。
