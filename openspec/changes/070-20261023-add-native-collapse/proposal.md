# Proposal

## Why

RynUI 缺少可保留编辑草稿的原生折叠面板。增加紧凑的首版 Collapse，让设置与详情页面按需展开内容，并遵守现有输入边界和 Theme 合同。

## What Changes

- 增加 typed CollapseProps、固定唯一 key 的 CollapsePanel，以及 title/body/optional extra slots。
- 支持受控/默认 activeKeys、多开/accordion、disabled、pointer/Enter/Space 与原生焦点反馈。
- 正文只挂载一次；隐藏释放布局空间，并由共同 branch 机制清理输入会话、捕获、焦点和子浮层。
- 接入尺寸及独立组件 Theme tokens，保持现有 Theme 成员、identity、订阅和黄金基线。
- 首版不含开合动画、动态面板拓扑、drag、服务、RTL/辅助功能桥接、完整 Web aliases；不修改 Gallery 或其他待集成组件。

## Capabilities

### New Capabilities

- `native-collapse`：保留式原生折叠面板、交互、主题和生命周期合同。

### Modified Capabilities

无。

## Impact

新增公开头文件、组件 host、Theme 集成、focused tests 与 docs/collapse.md；复用 logical layout/surface/icon/interaction，不改变 renderer、scene ABI 或第三方版本。风险集中在隐藏正文的交互清理与 callback 销毁，通过真实 retained Input 和 Modal/Spin headless 合同验证。使用独立工作区与本地提交，不 push、PR 或 archive。
