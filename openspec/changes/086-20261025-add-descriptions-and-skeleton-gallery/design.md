# Design

## Context

基线为 `bfbb49234c945c52988b8474838d3ac88bbc84ec`。Gallery 已有 334 个 stable IDs；正式组件提供 typed Props/slots、reactive Prop、retained content 和外部 ref。共同 `gallery_component_navigation_anchor` 已用于 Form/Date/Time 等真实示例。动机见 proposal。

## Goals / Non-Goals

**Goals:** 用少量原生控件组成完整编辑/加载流程；追加独立样例函数，既有函数体保持逐字一致；记录可重复的有限视口 headless 证据。

**Non-Goals:** 不改 Core/API/Theme/goldens/CMake，不修改全局 README/完成表，不引入样式入口或依赖，不把内部验收文本放入界面，不运行完整 frame/native 长矩阵。

## Decisions

1. 状态追加到既有 GalleryState，以 Signal 和 Binding 更新 Props；Input defaultValue 保存普通草稿，onChange 仅用于保存反馈，公开 InputRef 由外部编辑按钮实际使用。替代方案是重建整个样例，会破坏编辑状态，因此不采用。
2. Descriptions 固定 item 拓扑，名称 Input、角色 Select、启用 Switch 与保存 Button 分别位于 typed slots；布局控制和显隐恢复位于外部 Space。Skeleton 同样只挂载一次正文编辑器和 Select，loading 控制位于外部。三个 standalone 复用尺寸/形态状态。
3. 目录只扩展共同生产 anchor 的目标映射，不另写测试专用跳转；新增 fixture 通过真实目录 Button 事件、跳转、文本输入、选择和键盘操作验证。
4. 定向 `--details-loading-only` 接入完整 main 一次。复用现有 Recording fixture，检查实际 ordered scene glyph、surface 与 hit clip，逐区域断言以及四边焦点 coverage；保留 component/editor/scene/interaction 身份。
5. 正式构建使用 MSVC、Ninja Multi-Config、`windows-msvc-debug`，最多 4 jobs；只跑新 focused、必要旧代表、inventory、八 contracts 和 generator。平台通用逻辑以本机 Windows 结果记录；若需要 headless 边界验证，使用 `windows-msvc-headless` Debug/Release 定向组合，避免重复长矩阵。

## Risks / Trade-offs

- 尾部追加改变页尾 clamp → 必要旧代表检查实际按钮可见性，不能放宽旧行为断言。
- 窄列控件与装饰受限 → 控件使用有限宽度 Binding 并留 margin，测试真实 glyph/surface/hit clip 与四边 coverage。
- 测试误选同类控件 → 先按独立标题找到样例 owner，再按完整 ComponentId descendants 找真实控件。
- 构建缓存与锁定上游不在新树 → 仅使用可只读复制的本地来源，不联网；证据明确记录实际来源和未完成项。
- Headless 只证明逻辑场景 → GPU、native、Linux、系统 IME、AT 及父统一全套验收保持独立边界。
