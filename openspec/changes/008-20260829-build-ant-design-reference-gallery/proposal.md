## Why

当前 `rynui_token_gallery` 主要把 Theme、状态和 Token 验收数据压缩成一组 Button，既没有 Ant Design 文档式的信息架构，也会让非交互 Token 单元错误呈现 Button hover/focus 语义。RynUI 需要一份离线、版本化、可浏览的正式 Gallery，统一展示设计语言、完整组件分类、实现状态与真实可交互样例，避免后续开发反复在线检索并因关注点不同产生不一致实现。

## What Changes

- 将 Gallery 的正式上游快照锁定为 Ant Design `6.5.0` tag 与 commit `740ad964dc2397f33e40944367b0536a7314cc32`；官网当前版本只用于核对信息架构，不自动改变 RynUI 基线。
- 增加离线 `AntDesignReferenceCatalog`，以固定 identity 保存 Introduction 摘要、四项设计价值、资源入口、七个组件类别和 Ant Design 6.5.0 的完整 72 项组件清单。
- 把 Gallery 重构为文档式页面：提供标题、版本/来源、设计介绍、Foundation/Token 区、按 General、Layout、Navigation、Data Entry、Data Display、Feedback、Other 分类的组件总览，以及类别导航和支持状态筛选。
- 每个组件条目明确标记 `implemented`、`partial`、`planned`、`web-only`、`deprecated` 或 `out-of-scope`；已实现/部分实现项显示 RynUI 支持范围与真实样例，未实现项只展示事实状态，不伪造可用组件。
- 分离非交互 reference cell 与真实 Button：目录、说明、Token 和未实现组件条目不得注册 Button hover/click/focus；只有实际控件、导航和状态样例使用 Button 交互语义。
- 完整核对 Button 状态展示：Default outlined hover 保留 Ant Design 的 1px primary-colored border/text，Primary/Danger solid hover 只改变 fill；独立 1px gap + 3px focus-visible outline 只由 keyboard modality 显示。
- 增加适合长文档的 viewport/clip/滚动或等价分页导航、响应式宽窄布局、CJK/Latin 文本、键盘遍历、DPI 与真实窗口验收，使全部分类内容可到达而不压缩成不可读矩阵。
- 不在运行时抓取网页、不复制 Ant Design React/CSS-in-JS 实现或整段受版权保护文案；说明性内容使用简体中文摘要并保留官方来源链接。
- 非目标：本 change 不实现清单中的全部 72 个 RynUI 组件，不升级项目设计基线到 Ant Design 6.6.2，不发布浏览器/HTML renderer，也不把 Gallery 专用导航直接承诺为稳定公共 Application API。

## Capabilities

### New Capabilities

- `ant-design-reference-gallery`: 定义版本化离线 Gallery catalog、设计介绍、完整组件分类与支持状态、非交互/交互语义分离、文档导航、响应式呈现和跨平台验收合同。

### Modified Capabilities

无。当前 `openspec/specs/` 尚无已归档的 Gallery capability；Button、Theme、Flex 与 Space 的稳定公开合同不因本 change 改写。

## Impact

- 主要影响 `examples/token_gallery/`、Gallery manifest/generator、文档式布局与导航、Button 状态样例、headless tests、evidence schema 和 Windows/Linux 真实窗口证据。
- 可能增加仅供 Gallery 使用的非交互 retained surface、clip 与滚动/分页基础设施；任何稳定公共 API 扩展都必须在 design/tasks 中单独列出并通过 public header contract，不得借示例代码偷偷发布。
- 不新增第三方依赖；catalog 与摘要随源码版本化，运行时无网络访问。正式构建继续使用 CMakePresets、Ninja Multi-Config、Windows MSVC/D3D12/DXIL 与 Linux GCC/Clang/Vulkan/SPIR-V。
- 主要风险是把 Ant Design 6.6.2 live-site 内容混入 6.5.0 快照、把“列出组件”误报为“已经实现组件”、长页面无法访问、非交互条目继续污染 focus order，以及为了复刻网页而绕过 RynUI typed Theme/Component 边界。
- 可验证结果是 Gallery 在离线环境中展示来源版本、Introduction 摘要、七类 72 项组件和准确状态；所有内容可通过宽窄窗口与键盘/pointer 到达，只有真实交互样例显示 Button hover/focus，且 Windows/Linux 平台证据分别保存。
