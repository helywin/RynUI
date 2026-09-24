# Proposal

## Why

Switch/Checkbox 已验证共享窗口服务与按压机制；文本类控件的下一处复用边界是 Search。现有单行 Input 已有编辑、IME、selection 和提交能力，Button 已有按压、loading 和焦点能力，需要用真实组合验证它们能在同一窗口与布局中协作，而不复制编辑器或另建文本会话。

## What Changes

- 新增公开 `ryn::Search` 与 typed `SearchProps`，首批覆盖单行 value/defaultValue、placeholder、size/status、disabled/readOnly、maxLength、onChange、Enter 与按钮提交、`enterButton`、loading 和外部 `LayoutStyle`。搜索回调携带当前值与 `SearchSource::Input`；尚无 clear 入口时不伪造 `Clear` 来源。
- Search 组合现有 Input、Button、Flex 与窗口编辑服务；Input Enter 沿用既有 composition 门禁，按钮点击提交最后一次 committed value；loading、disabled/readOnly 时均不得误触发。受控模式与非受控模式保持 Input 既有语义。
- 在 Gallery 加入真实 Search 样例及 `partial` 支持状态，增加组合生命周期、提交、主题/缩放、scene identity 与空闲回归，并记录 Windows 实窗证据；Linux 专属验收保留独立待办。
- 首批不实现 `allowClear`、自定义 `searchIcon`/`enterButton` ReactNode 等价物、addonAfter、Password/TextArea/InputNumber 或完整表单集成；默认操作区先采用可读文字按钮，Gallery 明示当前覆盖范围。

## Capabilities

### New Capabilities

- `search-component`: Search 的公开 API、Input/Button 组合、提交来源、编辑与交互门禁、布局/视觉及分平台验收合同。

### Modified Capabilities

无；Input、Button、Flex 的既有可观察合同不变。

## Impact

- 新增公开头、组件组合实现、Gallery 样例和测试；必要时只对内部组合上下文增设窄接口，不创建第二份 `TextEditorStore`、IME session、clipboard 或 Button 按压状态机。
- 参考固定为本地 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/input/Search.tsx`、`style/search.ts` 与文档；现有 Theme/Component Token、`Prop<T>`、typed slots 和 phased invalidation 约束继续生效。
- 平台通用逻辑在一个受支持 preset 验证一次；Windows 的 Win32/D3D12/DXIL/系统字体/实际显示缩放与 Linux 原生 Wayland/Vulkan/SPIR-V/Fontconfig 分别验收，Linux 暂缓不代表通过。
