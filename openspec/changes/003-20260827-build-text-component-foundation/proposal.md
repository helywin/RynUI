## Why

`002-20260826-build-text-and-cjk-rendering-foundation` 已建立 UTF-8、字体 fallback、shaping、measurement、GlyphAtlas 与双平台 GPU 文本链路，但这些能力仍停留在 engine API，应用无法通过稳定的 `ryn::Text`、typed Props 和 reactive `Prop<T>` 声明文字。下一步需要先建立最小公开组件层，使后续 `Button`、`Input` 和 Device Monitor MVP 可以复用同一文本、布局、主题和失效合同，而不是各自直接拼接内部 renderer 对象。

## What Changes

- 增加公开 `Prop<T>` 值边界，使 typed Props 的字段可显式接受静态 `T`、`Signal<T>` 或 `Binding<T>`，并在组件 Scope 内建立确定订阅与清理。
- 增加公开 `LayoutStyle` 的首批外部布局字段和 typed component mount/slot composition 基础，使组件可以挂载到 retained Node 树而不公开内部 `NodeId`、layout model 或 SDL3 类型。
- 增加稳定的 `TextProps` 与 `ryn::Text` typed content 入口，把 `ryn::String` 接到既有 shaping、measurement、GlyphAtlas、Scene 和 renderer 链路。
- 增加最小 Theme/Token 读取边界：Text 只从 Theme 的 typography/alias token 取得稳定视觉值，公开 Props 不提供任意颜色、字体或 renderer style 覆盖；本 change 固定 Default token 快照，不实现完整主题算法和运行时主题切换。
- 以 Ant Design 6.5.0 Typography 的常规正文、次级和禁用语义作为首批视觉合同，并验证 content、tone、约束和外部布局变化保持最小失效范围。
- 提供公共 API、组件生命周期、reactive Prop、layout/measurement、token、真实窗口和双平台构建验收；保留后续 HitTest、Pointer、Focus 与 Button change 所需的组件边界。
- 非目标：完整 `Typography` 的 Title/Paragraph/strong、Button/Input 等交互组件、Pointer/Keyboard/Focus、完整 Theme Algorithm、Default/Dark/Compact 切换、Component Token override、ellipsis/copyable/editable、富文本、IME/Selection、结构响应 `If`/`For`、公开 `Flex`/`Space`、Accessibility 与多窗口。

## Capabilities

### New Capabilities

- `reactive-props`: 定义公开 `Prop<T>` 对静态值、`Signal<T>` 与 `Binding<T>` 的类型、订阅、Scope 生命周期及最小失效合同。
- `component-composition`: 定义公开组件挂载上下文、typed slot composition 与 `LayoutStyle` 外部布局边界，不暴露内部 Runtime/Layout/SDL3 类型。
- `text-component`: 定义 `TextProps`、语义化 Theme/Token 读取、文字测量与渲染、响应式更新及双平台真实窗口行为。

### Modified Capabilities

无。当前尚无归档到 `openspec/specs/` 的 main capability，本 change 不通过跨 change 路径修改已完成 change 的 delta spec。

## Impact

- 新增 `include/ryn/` 下的公开 Prop、component、layout style 与 text component API，并从 `rynui.hpp` 导出；扩展 `src/runtime/`、`src/layout/`、`src/text/`、`src/graphics/` 与 renderer controller 的内部挂载协议。Default Theme token 在本 change 保持内部只读快照，公开 Theme 配置入口留给后续 change。
- 需要把当前示例中手工组装的 engine Text state 收敛到可复用组件 host，同时保持 Font/Text/Graphics/Renderer 单向依赖和 owner-thread 规则。
- 不新增第三方依赖；继续使用 002 锁定的 FreeType、HarfBuzz、字体、SDL3 与 shader 工具链。
- 主要风险是 `Prop<T>` 所有权与临时对象生命周期、组件 Scope 清理、Text measurement 与 Node layout 双向耦合、Token 读取扩大失效范围，以及多个 Text 共享 atlas/renderer 时的稳定 range identity。
- 可验证结果是 consumer 只依赖公开 C++20 headers 即可编写 reactive Text content，仓库示例通过内部 application host 挂载同一公开 DSL；Windows/D3D12 与 Linux/Vulkan 真实窗口显示一致语义文本，content 更新只重塑目标 Text，tone 更新只刷新目标 Material，约束或 `LayoutStyle` 更新只触发必要布局，稳定状态不持续 submit。公开 `Application`/`Window` 入口留给独立 change，避免把临时资源配置固化成应用 API。
