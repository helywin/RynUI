## Why

`003-20260827-build-text-component-foundation` 已建立持久化 component identity、Node bounds、typed content、reactive `Prop<T>`、`LayoutStyle` 和公开 `ryn::Text`，但窗口输入仍只被聚合为“请求一帧”，应用无法命中组件、管理焦点或触发操作。下一步需要完成 `HitTest -> Pointer -> Focus -> Button` 的最小交互闭环，使 RynUI 从只读文字展示进入可操作的 MVP，同时为后续 Input、Scroll 和 Device Monitor 复用统一事件边界。

## What Changes

- 增加不暴露 SDL3 类型的平台输入归一化边界，首批覆盖鼠标主按键、主触点、键盘按下/抬起、窗口失焦和取消，并将 logical coordinates 与 owner-thread 约束传入 Runtime。
- 增加 generation-checked HitTest 与 Pointer router，按实际绘制顺序选择目标，支持 Capture、Target、Bubble 三阶段、传播停止、hover path 和 per-pointer capture；节点销毁、禁用或窗口失焦时安全取消 stale route。
- 增加单窗口 Focus manager，支持声明顺序的 `Tab`/`Shift+Tab` 遍历、pointer focus、focus-visible modality，以及 `Enter`/`Space` 的按钮键盘激活语义。
- 增加公开 `ControlSize`、`ButtonType`、`ButtonProps`、typed content slot 与 `ryn::Button`。首批稳定范围覆盖 Default/Primary、Small/Middle/Large、reactive disabled/loading、`onClick` 和 `LayoutStyle`；不开放任意颜色、边框、圆角或 renderer style。
- 扩展内部只读 Default Theme snapshot 与 scene service，映射 Ant Design 6.5.0 的 Button control height、padding、radius、default/primary、hover/active/focus-visible/disabled/loading 语义；Button content 通过受控前景色 context 复用公开 Text，不复制 shaping 或 GlyphAtlas。
- 使用 synthetic input/headless tests 证明 click 判定、捕获、焦点、销毁和最小失效边界；真实窗口示例验证 pointer/keyboard 操作、disabled/loading 抑制重复激活及 idle 后停止 submit。
- 非目标：公开 `Application`/`Window` 或原始 Pointer/Keyboard listener API、右键/手势/DragDrop、Accessibility、Button 的 Dashed/Text/Link/Danger/Ghost/Icon/Block/ButtonGroup、click wave、完整动画系统、公开 Theme override、Flex/Space/If/For、Input/IME 与多窗口焦点协调。

## Capabilities

### New Capabilities

- `input-routing`: 定义平台无关输入值、HitTest、三阶段 Pointer 路由、hover/capture、generation 与 owner-thread 生命周期合同。
- `focus-management`: 定义单窗口 focus order、focus-visible modality、键盘遍历/激活及销毁、禁用和窗口失焦处理。
- `button-component`: 定义公开 `ButtonProps`/typed content API、Default/Primary 与三种尺寸、disabled/loading/click 语义、Theme/Token 视觉和最小失效行为。

### Modified Capabilities

无。当前尚无归档到 `openspec/specs/` 的 main capability；Button 对 Text 前景色继承的要求在新 `button-component` capability 内定义，不回写未归档 change 的 delta spec。

## Impact

- 新增 `include/ryn/` 下的 Button 与 control value API，并从 `rynui.hpp` 导出；扩展 `src/platform/sdl/`、`src/runtime/`、`src/component/`、`src/graphics/` 和 scene renderer 的内部协议。
- 003 的 ComponentHost、Node bounds、Text child、Default Theme snapshot 与多个 Text scene service 将被复用；普通 pointer/focus/state 更新不得重新执行 content slot 或无关 Component。
- 不新增第三方依赖；继续使用已锁定的 SDL3、FreeType、HarfBuzz、字体和 shader 工具链。SDL event、scan code、window handle 与 GPU 类型不得进入公开 API。
- 主要风险是命中顺序与 scene 顺序漂移、回调期间销毁导致 stale identity、pointer capture 泄漏、键盘与 pointer 激活重复触发、Button 背景/Text/focus ring 的局部 dirty range，以及 loading/disabled 响应更新改变交互资格时的原子性。
- 可验证结果是同一个公开 Button DSL 在各平台独立清单中完成 build、CTest 和真实窗口证据；Linux 结果不替代 Windows，Windows 结果也不回退已完成的 Linux 项。
