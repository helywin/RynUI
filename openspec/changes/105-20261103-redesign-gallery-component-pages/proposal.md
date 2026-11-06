# Proposal

## Why

当前 Gallery 将 73 个目录条目与全部真实样例挂在同一长文档中，组件导航只移动滚动位置；日常查找组件、理解支持边界和比较样例都需要穿过无关内容，布局、输入命中与场景同步也承担无关组件的成本。需要参照 Ant Design 文档站的信息层级，在同一个原生窗口中建立真正独立的组件内容页，并以源码和 catalog 的真实记录表达实现进度。

## What Changes

- 固定顶部全局控制、可搜索/按状态筛选的分类导航、组件正文与页内 TOC；同一窗口切换当前页，保留总览、设计介绍和 Design Token 的独立入口。
- 锁定 Ant Design 6.6.5 的 7 类、73 个 canonical 条目全部有独立页面；implemented/partial 展示已登记的真实 C++ 样例，planned/web-only/deprecated/out-of-scope 提供诚实的说明页，不制造原生演示。
- 组件页按标题与状态、用途、示例卡、C++/公开 API、支持与缺失能力、Design Token 和来源组织；状态来自 support overlay，样例来自明确注册，不以任务勾选数、历史人工关闭项或 HTML 原型计算实现率。
- Input 的 Input/Search/Password/TextArea/OTP 使用一个 canonical 页面和五个样例分组；支持搜索别名，目录分母仍只计一个 Input。
- 新建 Gallery 内部页面模型、样例登记和页面生命周期控制，只挂载当前页面/当前复合家族分组。保留轻量编辑草稿及页内滚动位置，切页时取消旧页面的 popup、capture、focus、IME/preedit 与异步回调身份。
- 宽/中/窄窗口保留清晰排版、独立滚动、键盘可达性与 Default/Dark/Compact 主题；沿用 retained scene、局部 dirty 和 idle 合同。
- 将依赖全局序号、全页节点数量和长文档锚点的 Gallery 测试改为按 page/sample identity 定位；自动验收拆分平台通用逻辑与 Windows/Linux 原生集成。

本轮只创建规划和 `out/gallery-page-design/20261103/gallery-component-pages.html` 交互设计稿，不实现生产代码、不编译、不运行窗口或测试矩阵。HTML 是信息架构与交互示意，不是 RynUI 原生实现或视觉实测证据。

## Capabilities

2026-11-06 用户在全目录巡检完成后明确要求修复已报告的问题。本次后续实施覆盖 [巡检报告](../../../docs/reviews/2026-11-06-gallery-visual-audit.md) 的 B01–B03、V01–V14，并核对 S01–S04；以共用栈、布局、呈现和样例组合的实际原因修复，不升级 catalog、不新增组件。第 10 阶段的只读验收约束仅对应已经完成的巡检阶段，第 11 阶段已获实施授权。

### New Capabilities

- `gallery-component-pages`：离线 catalog 驱动的组件页面、真实支持范围、样例归属、响应式文档 shell、页面状态与生命周期隔离。

### Modified Capabilities

无。`openspec list --specs` 当前返回 `No specs found`，主 specs 尚无可修改的 capability。008 的长文档、032 的导航/窄窗口连续滚动属于历史 change；本提案明确演进这些行为，不修改历史产物，不伪造主 spec 的 MODIFIED delta。

## Impact

- 后续实施主要涉及 `examples/token_gallery/` 的 definition、document model/viewport、scroll/layout/runtime、ReferenceSurface 和新增内部 page/sample 文件；调整 `examples/CMakeLists.txt` 与对应测试注册。
- 沿用 `gallery/ant-design/6.6.5/source-manifest.json`、`support-overlay.json` 及离线生成器；不升级上游版本、不凭本次页面改造修改组件支持状态。
- 使用现有 `include/ryn/` 的 typed Props/slots、`Prop<T>`、Theme/Component Token。页面控制器和必要的内部 mount adapter 不成为公开 Router、Modifier、ScrollArea 或新组件 API。
- 不引入 Web renderer、HTTP server、运行时网页抓取、VDOM、SDL 类型泄漏、组件私有 GPU 上传或新依赖。
- 主要风险是拆分混合样例时丢失流程、页面销毁后残留输入/弹层、草稿与 renderer/editor 生命周期混淆，以及旧测试过度依赖全局挂载顺序。验收通过全目录路由、旧样例唯一归属、事件代际、资源退役、局部失效与定向平台集成检查覆盖。
- 可验证结果是 73/73 canonical 页可达、声明的样例可按页操作、非活动页无挂载节点/命中/动画工作、切页后旧输入不能影响新页；不承诺未经测量的性能百分比或固定 FPS。
