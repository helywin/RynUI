## Why

RynUI 已有文本 shaping、HitTest、Focus、Theme 和按需渲染，但还没有可编辑文本、IME composition 或 Ant Design `Input`，因此桌面表单、Gallery 搜索以及中文输入都无法建立真实交互闭环。架构 Phase 3 已把 TextInput、IME、Selection、Clipboard 与 Undo/Redo 列为下一阶段，本 change 先建立可验证的单行文本输入基础。

## What Changes

- 增加平台无关的单行文本编辑模型：以有效 UTF-8 内容为边界，保存 grapheme-safe caret/selection、IME composition、有限 undo/redo history，并提供确定性的 insert/delete/move/select 操作。
- 扩展平台 input value 与 SDL3 adapter，归一化 `SDL_EVENT_TEXT_INPUT`、`SDL_EVENT_TEXT_EDITING`、`SDL_EVENT_TEXT_EDITING_CANDIDATES`、keyboard repeat、clipboard 和 `SDL_SetTextInputArea`，SDL3 类型不得泄漏到公开 API 或组件层。
- 增加公开 `ryn::Input`、typed `InputProps` 与 `Prop<T>` value/status/disabled/readOnly，支持 placeholder、prefix/suffix slots、受控值、`onChange`/`onSubmit` 回调及 keyboard/pointer focus 行为。
- 按锁定 Ant Design 6.5.0 设计 `Input` 的 small/middle/large、default/hover/active/focus-visible、disabled、read-only、warning/error 状态，视觉只消费 Theme 与 Input Component Token；caret、selection、composition underline 和 focus ring 使用 retained scene primitives。
- 接入系统 clipboard 与 IME 生命周期：focus/blur/window focus/destroy 时严格 start/stop，候选窗位置跟随 logical caret 与 DPI，composition 只作临时呈现，commit 后才进入 value/history。
- 增加平台通用 unit/headless/contract/benchmark 与分平台真实窗口 evidence；平台通用合同只在一个受支持平台验收，Windows 和 Linux 的 IME、clipboard、system font、DPI/window-system 行为分别验收。
- 非目标：本 change 不实现多行 `TextArea`、password/security storage、rich text、bidirectional visual caret、drag-and-drop、Accessibility 平台桥接、mobile virtual keyboard 专项适配、Form validation framework 或 Gallery 自由文本搜索；这些必须在后续 change 中单独设计。

## Capabilities

### New Capabilities

- `text-editing-runtime`: 定义有效 UTF-8 文本、caret/selection、composition、clipboard、undo/redo、输入会话与平台事件归一化合同。
- `input-component`: 定义 Ant Design 6.5.0 风格的单行 `ryn::Input` 公开 API、布局、状态、渲染、交互和响应式更新合同。

### Modified Capabilities

无。

## Impact

- 公开 API：新增 `include/ryn/input.hpp` 并由 `include/ryn/rynui.hpp` 导出 typed `InputProps`、相关枚举和 `Input` component entry；继续使用现有 `ryn::String`/`StringView` 作为公开文本边界。
- Runtime：新增 internal text editing/session/history 模块，扩展 Focus、Pointer、Text scene、Dirty domain 和 retained selection/caret primitives，不引入 Virtual DOM 或通用 `Modifier`。
- Platform/Unicode：扩展 `src/platform/` 的 SDL3 event adapter、text input area 与 clipboard bridge；SDL3 3.4.14 继续由现有 lock 管理。新增 utf8proc 2.11.3 作为 internal Unicode grapheme segmentation 依赖，必须使用显式 `BUNDLED|SYSTEM`、固定 source SHA256/license，并且不得进入公开 API。
- Theme/Gallery：补齐 Input Component Token runtime mapping 与真实 Input sample；只有完成对应 evidence 后才能把 Gallery 的 Input 状态从 `planned` 更新为 `partial`。
- 验证：Windows 使用 MSVC/D3D12/DXIL 与系统中文 IME，Linux 使用原生 Wayland/Vulkan/SPIR-V 与桌面 IME；两端证据和 checkbox 独立保存。
