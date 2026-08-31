## Context

参见 `proposal.md` 的 Why。当前 RynUI 已有有效 UTF-8 `ryn::String`、HarfBuzz shaping/fallback、retained Text scene、generation-safe Component/Node/Interaction、Pointer/Focus、Theme、Animation deadline 和 SDL3 platform adapter，但 `String` 是不可变 consumer value，系统没有编辑 buffer、selection、IME session、clipboard bridge 或可输入组件。

锁定 SDL3 3.4.14 已提供显式 start/stop text input、UTF-8 committed/editing/candidate events、window-coordinate text input area 与 clipboard API；这些调用只允许 main thread，且 event 内字符串/候选列表必须在 event 生命周期内复制。`SDL_TextEditingEvent::start/length` 使用 UTF-8 character 数量，不是 byte offset。SDL3 类型继续只存在于 `src/platform/`。

字体 shaping cluster 受字体和 fallback 影响，不能单独承担 emoji ZWJ 与 combining sequence 的编辑边界。utf8proc 2.11.3 提供 UAX#29 extended grapheme break、UTF-8 decode 和 Unicode 17 数据，体积较小且为 MIT + Unicode data license，适合作为 internal `BUNDLED|SYSTEM` dependency；它不负责 shaping、normalization 或公开字符串类型。

## Goals / Non-Goals

**Goals:**

- 建立单行 editor 的 value、grapheme boundary、selection、composition、history 和 controlled reconcile 单一状态机。
- 让 SDL3 main-thread text input/IME/clipboard 通过纯 Ryn value event 接入现有 Focus/Pointer 生命周期。
- 发布符合现有 typed Props/slots/`Prop<T>` 模式的 `ryn::Input`，并让视觉、布局和动画严格消费 Ant Design 6.5.0 Token。
- 复用 retained scene、dirty phase 和按需 frame loop；steady-state caret/selection/composition 不 remount 或按 frame 分配。
- 把平台通用合同、Windows native evidence、Linux native evidence 分开。

**Non-Goals:**

- 不把 internal editor、IME candidate UI、clipboard service、utf8proc 或 SDL3 类型导出到 `rynui.hpp`。
- 不在本 change 解决多行 layout、bidirectional visual navigation、password/security、rich text、Accessibility 或 mobile virtual keyboard 专项行为。
- 不实现自绘 IME candidate popup；系统候选窗由平台定位 API 管理，candidate metadata 只用于 diagnostics/test。
- 不在本 change 启用 Gallery 自由文本搜索；Input 成为可验收 live sample 后再开独立 search change。

## Decisions

### 1. 使用 utf8proc 2.11.3 建立与字体无关的 grapheme boundary map

新增 `cmake/dependencies/RynUIUtf8proc.cmake`，在 `RynUIDependencyLock.cmake` 集中保存 version、source URL、SHA256 与 `MIT AND Unicode-3.0` license。依赖模式只接受显式 `BUNDLED|SYSTEM`：

- `BUNDLED` 使用 release archive 和 `FetchContent`，关闭 tests/install/shared build，并归一为 internal CMake target。
- `SYSTEM` 要求可验证的 2.11.3 compatible package/target；缺包、版本不符或 target 不符立即失败。
- 不使用 Git submodule，不做 system-first fallback，不把 include path 或 C type 暴露给 public target。

`TextBoundaryMap` 以 UTF-8 byte offset 保存 `[0, ..., size]` 边界，构建时用 `utf8proc_iterate` 验证 scalar，再用 `utf8proc_grapheme_break_stateful` 生成 UAX#29 extended grapheme boundaries；另保存 byte-to-scalar prefix index，用于 SDL composition character range 与 `maxLength` 映射。value 未变化时 selection/caret update 复用 map。

备选方案：只用 HarfBuzz glyph cluster 会随 font/fallback 改变且可能拆分 emoji；自带 Unicode tables 会扩大生成器、数据更新和合规面；按 Unicode scalar 编辑会错误拆分用户感知字符。因此选择 utf8proc。

### 2. `TextEditorState` 是平台无关的唯一编辑事实来源

在 `src/input/` 增加 internal `TextEditorState`，保存：

```text
committed UTF-8 buffer + TextBoundaryMap + revision
selection {anchor_byte, caret_byte}
composition {text, selection_scalar, candidates, selected, orientation}
horizontal caret target / merge epoch
bounded undo/redo transactions
controlled echo {emitted value, edit revision}
```

所有 mutation 先在 temporary transaction 中验证 UTF-8、boundary、maxLength 和容量，再一次提交；失败不会留下 partial value/selection/history。selection 使用 byte offsets 便于 splice，但任何 public snapshot 和 command 都只能产生 boundary map 中的 offset。

history 默认上限为 128 transactions 与 1 MiB retained text payload；超过预算时逐个淘汰最旧完整 transaction，单个 transaction 本身超过预算则原子拒绝。连续 committed text 只在相同 merge epoch 合并；selection jump、clipboard、composition commit、submit、external reconcile 和 undo/redo 都推进 epoch。controlled echo 以 value equality + pending edit revision 识别，不依赖 callback 时序或对象地址。

备选方案：每次修改使用完整 persistent rope 会过度设计当前单行边界；直接修改 caller `Signal` 会破坏 Props 单向数据流；依赖 wall-clock 合并 typing 会让测试和 replay 不确定。

### 3. controlled 与 uncontrolled 模式在 mount 时固定

`InputProps` 使用现有 `ControlSize`，并新增 `InputStatus {Default, Warning, Error}`。建议公开形状为：

```cpp
class InputProps final {
public:
    InputProps& value(Prop<String>);
    InputProps& defaultValue(String);
    InputProps& placeholder(Prop<String>);
    InputProps& status(Prop<InputStatus>);
    InputProps& disabled(Prop<bool>);
    InputProps& readOnly(Prop<bool>);
    InputProps& maxLength(Prop<std::size_t>);
    InputProps& onChange(std::function<void(String)>);
    InputProps& onSubmit(std::function<void(String)>);
    InputProps& layout(LayoutStyle);
};

struct InputSlots final {
    std::optional<InputPrefix> prefix;
    std::optional<InputSuffix> suffix;
};

void Input(InputProps props, InputSlots slots = {});
```

callback 按 value 传递 owned `String` snapshot，避免 `StringView` 生命周期隐患。`value` 与 `defaultValue` 同时出现时 mount fail-fast；controlled mode 的 committed edit 只产生 pending presentation + callback，caller echo 后确认 authoritative value；uncontrolled mode 直接更新 owner-local value。模式是 generation identity 的一部分，改变模式必须 remount。

prefix/suffix 是不同 tag 的 typed `SlotContent`，内容只在 mount/structure 更新执行；它们不是 `InputProps` 中的 visual callback。`LayoutStyle` 只应用 root，Input 内部 padding/radius/color 不开放覆盖。

备选方案：只做 controlled mode 会让最小 demo 必须携带 Signal glue；只做 uncontrolled 会破坏 declarative value source；把 slots 放进通用 children list 会丢失语义和布局位置。

### 4. platform text input 通过 generation-safe session bridge 路由

新增纯 Ryn internal event：

```text
TextCommitted {String text}
CompositionChanged {String text, scalar start, scalar length}
CandidatesChanged {vector<String>, selected, horizontal}
ClipboardUpdated {owner, mime types diagnostics}
```

`SdlEventAdapter` 在 dispatch 返回前复制 event string/candidate list，验证 UTF-8 后送入 `TextInputSessionHost`。session host 只保存当前 window 的 `TextInputOwnerId {slot,generation}` 与 command sink；每次 dispatch、clipboard completion、caret update 和 stop 都重新验证 identity/owner thread。

Focus 获得后使用 text type、autocorrect、multiline=false、placeholder/default/max length properties 启动 SDL input；blur、window focus loss、disabled、destroy 先清 composition/capture/deadline，再 stop session。IME 可能吞掉普通 key event，因此 committed character 永远只从 text event进入 editor，key event只负责 navigation/shortcut/submit/cancel。

`SDL_SetTextInputArea` 的 rectangle/cursor 使用 Node global logical bounds、ancestor translation/clip 与 active render scale 转换到 window coordinate；rectangle 向外取整，cursor 相对 area.x clamp。layout、scroll、viewport、DPI 或 caret 改变会 dirty `InputArea`，同一数值不重复调用平台。

备选方案：让 Component 直接调用 SDL 会泄漏平台依赖并破坏 headless test；按 pixel coordinate保存 caret 会在 DPI/reflow 后漂移；共享无 generation 的全局 focused pointer 会接受迟到事件。

### 5. keyboard、pointer 与 clipboard 复用现有 Focus/Pointer 路由

focused Input 在 FocusManager 的 text-edit command stage 先处理 editing shortcuts，再让 Tab/Escape 等未消费 command 继续全局路由。platform adapter 把 primary shortcut modifier 归一为 `Control`（Windows/Linux）或未来 macOS 的 `Command`，并保留 key repeat；Enter 只在 key-down、non-repeat、无 composition 时 submit。

Pointer down 通过 `TextCaretMap` 的 glyph cluster x-ranges选择最近 boundary并 capture；drag 更新 anchor/caret，double-click 使用 utf8proc category + whitespace/punctuation rule 选择 logical word。当前 change 的 Left/Right 和 selection geometry按 logical LTR/CJK 顺序，bidirectional visual navigation 留给后续 capability。

clipboard bridge 的 `get/set/has text` 只在 platform main thread执行，取得的 UTF-8立即复制为 `String` 并释放平台内存。copy/cut 对 selection snapshot 操作；paste 去除 CR/LF，完整验证后作为单个 transaction提交。read-only 允许 selection/copy，禁止 cut/paste/value command。

### 6. Text shaping 输出独立 `TextCaretMap`

现有 TextEngine 在 shaping 后增加与 `ShapedText` revision绑定的 caret data：每个 grapheme boundary 的 logical byte offset、x advance、glyph range 和 line baseline。utf8proc boundary决定合法编辑位置，HarfBuzz glyph cluster只提供 visual x mapping；多 glyph/ligature内部的多个 grapheme boundary按该 cluster advance等比分配作为 foundation fallback，并通过 Latin ligature、combining、CJK、emoji测试锁定。

committed value + composition 通过临时 display buffer shape；caret/selection映射回 committed byte offset与 composition scalar range。value 未变化而 selection、caret blink 或 material变化时不重新 shape。

备选方案：从 glyph index反推 byte boundary会混淆 fallback cluster；每次 pointer move重新 shape会制造分配和无关 dirty；使用等宽字符宽度无法支持 CJK/ligature。

### 7. Input 使用固定 retained scene topology 与内部 horizontal viewport

Input mount 创建稳定 root、prefix、editable viewport、suffix 与以下 retained layers：container quad、border/active shadow/focus effect、selection background、base glyph run、selected glyph overlay、placeholder、composition underline、caret。inactive layer 通过 opacity/empty range隐藏，不在 steady-state增删 fragment。

宽度不足时 prefix/suffix先按 intrinsic constraints布局，editable viewport占剩余宽度并启用 ancestor clip；`scroll_x` 只更新 editable subtree Transform/HitTest/input-area，不改变 root Measure。caret-visible算法在 padding 后 viewport内保留一个 logical margin，并在 value/reflow后 clamp。

selection background位于 container 之上、glyph之下；selected glyph overlay仅绘制 selection clip内的 glyph color；composition underline和caret位于 glyph之上。所有 effect继续使用现有 rounded-effect renderer，不新增 Input 私有 shader。

### 8. Input Component Token 独立解析并复用 MotionPolicy

新增 internal `InputTokenSet`，从锁定 Ant Design 6.5.0 Seed/Map/Alias 与现有 catalog runtime mapping解析：control heights、font sizes、padding inline/block、border/radius、hover/active border/background、active shadow、warning/error shadow、disabled colors、selection/caret colors。Component Token override只影响 Input consumer，不改 Seed 名录。

hover/status/material过渡使用 `motionDurationMid + motionEaseInOut`；focus-visible与caret placement即时更新。caret blink由 internal deadline controller驱动，输入/selection后重置；blur/disable/window focus loss立即移除 deadline。Theme motion disabled或effective reduced policy显示静态 caret，不做持续 blink。

备选方案：复用 Button token会复制目前已发现的 hover/focus边框错误；公开 `PrimitiveStyle`会绕过 Theme contract；固定 16ms timer会破坏 idle。

### 9. invalidation 按内容、几何、材质和 input-area 分开

- authoritative/committed/composition text变化：validate -> boundary -> shape；只有 intrinsic尺寸变化才进入 Measure/Layout。
- selection/caret：Geometry/Material；pointer placement需要当前 caret map但不修改 structure。
- horizontal scroll：Transform + HitTest + InputArea。
- hover/focus/status/Theme color：Material/Effect；font/size token变化才重新 shape/Measure。
- prefix/suffix结构：Component/Measure/Layout，仅对应 slot subtree。

Input diagnostics记录 edit/composition/history/session/caret-map/shape/material/geometry/input-area/deadline counters。benchmark预留 value/boundary/history/scene容量，验证一万次 selection/composition update不增长 topology或触发 sibling work。

### 10. 验收分为平台通用、Windows 与 Linux

平台通用任务在一个受支持 preset运行一次：utf8proc resolver/lock/license、Unicode boundary、editor、history、controlled reconcile、Input API/layout/Token/scene、headless pointer/keyboard/IME fake events、allocation/benchmark、public dependency和 evidence schema。

Windows 独立使用 `windows-msvc` + Ninja Multi-Config，验证 MSVC x64、SDL3 Win32 text input、系统中文 IME candidate位置、clipboard、Ctrl shortcuts、DirectWrite system font discovery、D3D12/DXIL及1.0/1.25/1.5/2.0 DPI真实窗口。

Linux 独立使用 `linux-gcc`/`linux-clang`，在原生 Wayland上验证系统 IME、clipboard、Ctrl shortcuts、Fontconfig、Vulkan/SPIR-V与至少两档实际 scale；不得用 X11或Windows代替。平台 checkbox与 evidence独立提交。

## Risks / Trade-offs

- [utf8proc SYSTEM package 的 CMake target/version metadata 不统一] → resolver只接受显式可验证组合并归一 internal target；不做静默 fallback，contract覆盖缺包/错版本/错 target。
- [Unicode grapheme 与 HarfBuzz visual cluster不一一对应] → logical boundary与visual caret map分层，ligature内使用稳定 fallback分配；复杂 bidi visual navigation明确留后续。
- [IME 在不同平台的 commit/cancel与 candidate event顺序不同] → composition作为临时快照、committed text作为唯一 value mutation，使用 fake adapter permutation tests加各平台真实 journey。
- [controlled callback延迟造成caret跳动] →保存单个 pending edit revision并识别等值 echo；不同 external value按冲突 reconcile，不无限缓存 speculative edit。
- [history复制完整短字符串可能放大内存] →单行 foundation使用128 transaction/1 MiB双上限与oldest eviction；长文本rope在性能证据证明必要后另开 change。
- [caret blink造成持续唤醒或视觉抖动] →只在有效 focused owner启用deadline，reduced/disabled/blur立即静态化并通过idle-after-caret测试。
- [selection/IME视觉可能被DPI clip] →geometry以logical坐标保存、提交前统一scale/round，四档simulated geometry和各平台真实窗口分别验收。

## Migration Plan

1. 先集成并锁定 utf8proc、Unicode boundary与平台无关 editor，不导出公开 API。
2. 增加纯 Ryn text input events/session fake bridge和SDL3 adapter，保持现有 Button key route兼容。
3. 发布 `ryn::Input` API、retained scene、Token与headless journey；现有 consumer无需迁移。
4. 在 Gallery加入真实 Input sample并把 overlay由 `planned`改为`partial`，前提是相应 capability/evidence已通过。
5. Windows和Linux分别完成native evidence；任一平台失败只回滚对应platform adapter/evidence，不删除已通过的平台通用合同。
6. 若整体需要回滚，移除Input导出和新增internal target；依赖lock、生成文件和Gallery status随同同一revert恢复，不改变现有Button/Text/Flex/Space ABI。
