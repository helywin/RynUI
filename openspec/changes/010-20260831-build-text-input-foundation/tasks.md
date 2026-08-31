## 1. utf8proc 与 Unicode grapheme boundary

- [ ] 1.1 在集中 dependency lock 中固定 utf8proc `2.11.3` 的 release archive、source SHA256、`MIT AND Unicode-3.0` license 与 normalized target；实现显式 `BUNDLED|SYSTEM` resolver，关闭无关 build 目标，并通过缺包、错版本、错 target、离线 source override、license 和 public link-interface contracts 验证不使用 Git submodule 或 system-first fallback
- [ ] 1.2 实现 internal UTF-8 scalar iterator 与 `TextBoundaryMap`，使用 `utf8proc_iterate`/`utf8proc_grapheme_break_stateful` 保存 byte/scalar/grapheme 映射；通过 invalid UTF-8、ASCII、Latin combining、CJK、emoji modifier/flag/ZWJ、empty、large offset 和 Unicode version tests 验证
- [ ] 1.3 引入锁定 Unicode 17 `GraphemeBreakTest` validation fixture 或等价 checked-in generator input identity，使用 `python -B` 验证生成/fixture SHA、UAX#29 boundary corpus 和 stale output rejection；运行 tracked/untracked `__pycache__`、`.pyc`、`.pyo` contract
- [ ] 1.4 在一个受支持平台使用正式 preset 构建并运行 utf8proc resolver、boundary、dependency lock/license/public dependency tests 与 `git diff --check`；以英文 `build: add unicode text boundaries` 提交并推送本阶段，核对 remote SHA

## 2. 平台无关 TextEditorState 与 selection

- [ ] 2.1 实现 generation-safe internal `TextEditorState`、owned committed UTF-8 buffer、revision、grapheme-aligned `{anchor,caret}` selection 与 atomic mutation transaction；通过 invalid owner、destroy/reuse、wrong-thread、empty/value replacement 和 failure rollback tests 验证
- [ ] 2.2 实现 insert/replace、Backspace/Delete、Left/Right、Home/End、Shift extension、Select All、pointer placement 与 collapse selection 规则；通过 ASCII、combining、CJK、emoji、selection direction、boundary clamp、disabled/read-only tests 验证不拆分 grapheme
- [ ] 2.3 实现 logical word selection、single-line CR/LF removal 与 Unicode scalar `maxLength`，锁定 double-click word、punctuation/whitespace、paste newline、partial IME commit 和 cluster-safe truncation tests
- [ ] 2.4 增加 editor diagnostics、reserve/capacity seam 与 10,000 次 caret/selection/mutation benchmark；证明 value 不增长时预热后无 heap allocation、identity/capacity 不按操作增长且不产生 Component/Scene side effect
- [ ] 2.5 运行 editor state、selection、Unicode boundary、owner-thread、allocation/benchmark 与 `git diff --check`；以英文 `feat: add text editor state` 提交并推送本阶段，核对 remote SHA

## 3. IME event 与 text input session bridge

- [ ] 3.1 增加不含 SDL3 类型的 `TextCommitted`、`CompositionChanged`、`CandidatesChanged` value events 与 batch ordering contract；进入队列前复制 `String`/candidate snapshot，通过 invalid UTF-8、scalar range、empty composition、candidate selection/orientation 和 bounded capacity tests 验证
- [ ] 3.2 实现每 window 单 owner 的 generation-safe `TextInputSessionHost`，把 Focus、window focus、disabled/read-only、destroy/reuse 与 start/stop/cancel composition 收口；通过 focus transfer、conditional unmount、late event、duplicate start/stop 和 owner-thread tests 验证
- [ ] 3.3 扩展 SDL3 adapter 归一化 `SDL_EVENT_TEXT_INPUT`、`SDL_EVENT_TEXT_EDITING`、`SDL_EVENT_TEXT_EDITING_CANDIDATES` 与 keyboard repeat，复制 event-lifetime 数据并把 character range 映射为 internal scalar range；通过 fake SDL-shaped structs、batch ordering、capacity、error propagation 与 forbidden-include tests 验证
- [ ] 3.4 实现 text input properties 与 logical caret/input bounds 到 window-coordinate `SDL_SetTextInputArea` 的 scale/translation/clip/rounding adapter；通过 1.0/1.25/1.5/2.0 scale、scroll/reflow、window focus、same-value elision、negative/offscreen clamp tests 验证
- [ ] 3.5 将 composition transient state 接入 editor，确保 update/candidate 不修改 committed value/history，commit 形成单 transaction，cancel/blur/destroy 清理；通过多事件 permutation、中文拼音、韩文组合、emoji composition、controlled external conflict tests 验证
- [ ] 3.6 运行 session、platform input、Focus、event batch、composition、input-area、public dependency 与 `git diff --check`；以英文 `feat: add text input session bridge` 提交并推送本阶段，核对 remote SHA

## 4. Clipboard、history 与 controlled reconcile

- [ ] 4.1 增加 platform-independent UTF-8 clipboard contract 与 SDL3 main-thread bridge，正确复制/释放 platform text 并归一化 clipboard update metadata；通过 empty/error、invalid UTF-8、copy ownership、thread、platform lifetime 和 no-SDL-leak tests 验证
- [ ] 4.2 实现 copy/cut/paste command，使用 command-time selection snapshot、read-only eligibility、CR/LF removal 与单 transaction replace；通过 clipboard mutation race、selection change、large paste、maxLength、platform failure atomicity tests 验证
- [ ] 4.3 实现 128 transaction/1 MiB 双上限 undo/redo history、merge epoch、oldest eviction 与 branch clearing；通过 contiguous commit、selection/paste/cut/composition/submit break、oversize reject、undo selection restore 和 20,000 次 history benchmark 验证
- [ ] 4.4 实现 authoritative controlled echo 与 external conflict reconcile，识别相同 emitted value/revision并保留 caret/history，不同 value 取消 composition、clamp selection且不回调循环；通过 delayed echo、duplicate value、external shorter/longer value、undo 后 echo 和 owner reuse tests 验证
- [ ] 4.5 运行 clipboard、history、reconcile、composition integration、allocation/benchmark 与 `git diff --check`；以英文 `feat: add text editing history` 提交并推送本阶段，核对 remote SHA

## 5. 公开 Input API、生命周期与布局

- [ ] 5.1 增加 `include/ryn/input.hpp` 的 `InputStatus`、typed `InputProps`、typed prefix/suffix slots、controlled `value`、`defaultValue`、placeholder/status/disabled/readOnly/maxLength、owned `onChange`/`onSubmit` callback 与 `LayoutStyle`；从 `rynui.hpp` 导出并通过 compile-smoke、header isolation、narrow string/renderer/SDL/Modifier compile-fail contracts 验证
- [ ] 5.2 在 props mount 时拒绝 value/defaultValue 冲突并固定 controlled mode；实现 generation-checked Input component record、editor/session/interaction ownership 与 destroy/reuse cleanup，通过 invalid props、conditional mount、Focus/capture/composition/deadline teardown 和 callback self-destroy tests 验证
- [ ] 5.3 实现 small/middle/large root、prefix/editable viewport/suffix Constraints layout、intrinsic measure、baseline、clip 与 horizontal scroll slot；通过 empty/placeholder/CJK/Latin/long text、missing/双 slot、窄宽、min/max constraints、四档 simulated DPI tests 验证
- [ ] 5.4 接入 `Prop<T>` subscription，证明 value/placeholder/font/size/slot只触发必要 Shape/Measure/Layout，disabled/readOnly/status只触发对应 Interaction/Material，普通更新不重跑 slots 或 sibling Component；覆盖 nested Theme、Signal batch 和 unmount tests
- [ ] 5.5 运行 public API、Component lifecycle、Props、Layout/Constraints、Text shaping、Focus/Pointer、dependency leak 与 `git diff --check`；以英文 `feat: add input component API` 提交并推送本阶段，核对 remote SHA

## 6. TextCaretMap 与 retained Input scene

- [ ] 6.1 扩展 TextEngine 输出 revision-bound `TextCaretMap`，把 utf8proc grapheme boundary 映射到 HarfBuzz glyph cluster x-range；通过 Latin ligature、combining、CJK fallback、emoji ZWJ、missing glyph、empty与重复 x tests 验证 logical caret/nearest hit
- [ ] 6.2 实现 committed + composition display buffer、selection/caret映射、placeholder visibility 与 horizontal caret-visible scroll；通过 composition scalar range、selection across composition、Home/End、value shrink、viewport resize和controlled echo tests验证不重复 shape
- [ ] 6.3 建立固定 retained scene topology：container/border/effects、selection background、base/selected glyph、placeholder、composition underline与caret，接入 ancestor clip/order；通过 layer identity、opacity hide、destroy/reuse、Theme、clip和 CPU scene reference tests验证steady state不增删 layer
- [ ] 6.4 接入 Quad/RoundedEffect/Glyph GPU range，验证 selection/glyph/effect draw order、DPI pixel alignment、caret/underline thickness、range upload、deferred retry和 shader contract；四档 scale下不得裁切 glyph、caret、shadow或focus effect
- [ ] 6.5 增加 256 Input / 20,000 selection-composition update benchmark和 frame diagnostics，证明固定 scene capacity、无 Structure/Measure/Layout/无关 HitTest、最小 GPU dirty range与预热后 hot path 0 heap allocation
- [ ] 6.6 运行 TextCaretMap、Input scene、clip/order、renderer/GPU、frame integration、benchmark与 `git diff --check`；以英文 `feat: render editable input text` 提交并推送本阶段，核对 remote SHA

## 7. Ant Design Token、交互与 caret deadline

- [ ] 7.1 增加锁定 Ant Design 6.5.0 Input source contract与internal `InputTokenSet`，解析 control heights、font/padding、border/radius、hover/active/status/disabled、selection/caret和shadow；通过direct value、Token identity、Default/Dark/Compact/nested override与生成文档stale tests验证
- [ ] 7.2 实现 default/hover/active/focus-visible/disabled/read-only/warning/error retained materials和`motionDurationMid + motionEaseInOut`过渡；通过Default hover仅既有border、pointer focus无额外ring、keyboard focus、warning/error active shadow与rapid retarget tests验证
- [ ] 7.3 实现pointer click nearest caret、drag selection/capture、double-click word、leave/release/cancel和Input内部scroll hit同步；通过CJK/Latin/emoji、prefix/suffix bounds、outside release、multi-pointer和allocation tests验证
- [ ] 7.4 实现keyboard navigation、Shift selection、primary shortcut A/C/X/V/Z/Y、Backspace/Delete、Enter submit、Escape composition cancel与repeat priority；通过Windows/Linux modifier-shaped fake events、IME吞key、read-only/disabled、callback destroy和focus traversal tests验证
- [ ] 7.5 实现retained caret blink deadline，input/selection时重置，blur/window focus loss/disabled/destroy时移除，Theme motion disabled或reduced时显示静态caret；通过controlled clock、60/120/144 Hz、no polling、last-caret idle与GPU submit diagnostics验证
- [ ] 7.6 运行Input Token/state、Pointer/Focus/keyboard、clipboard/history、Animation/frame idle、GPU reference和 `git diff --check`；以英文 `feat: add input interactions and tokens` 提交并推送本阶段，核对 remote SHA

## 8. 平台通用 Input 集成与 Gallery

- [ ] 8.1 增加完整headless Input journey，覆盖controlled/uncontrolled、Latin/CJK/emoji、composition/candidates/commit/cancel、selection、clipboard、undo/redo、maxLength、slots、Theme/status、caret blink、external reconcile、owner destroy和恢复idle；验证逐阶段value/selection/identity/dirty/scene diagnostics
- [ ] 8.2 在真实 `rynui_token_gallery` 的 Live Samples加入 `ryn::Input` interaction sample与能力/缺失范围说明；只有Input runtime/API/headless evidence可解析后才把72项overlay中的Input从`planned`更新为`partial`，未实现TextArea/password/search不得伪装为支持
- [ ] 8.3 增加Input evidence schema，要求utf8proc/Unicode identity、API/mode、value/selection/composition/history、clipboard/session/input-area、Token/scene/upload/frame/idle、preset/compiler/platform/window/driver/shader/font/IME/scale与人工确认路径；拒绝planning-only、跨平台identity和缺失commit/idle证据
- [ ] 8.4 在一个受支持平台用正式preset运行全部平台通用unit/headless/contract/benchmark、public dependency、lock/license、无网络runtime和Python cache检查，记录实际OS/compiler/preset/result，不要求另一平台重复本组合同
- [ ] 8.5 更新`docs/architecture.md`与必要generated Token reference，记录Input/editor/session/utf8proc边界；README只更新用户可见能力与文档入口，AGENTS工作流不得混入README
- [ ] 8.6 运行`openspec doctor --json`、`openspec validate --all --strict --no-interactive`与`git diff --check`；以英文 `test: validate input component contracts` 提交并推送平台通用evidence，核对remote SHA

## 9. Windows 专属验收

- [ ] 9.1 使用`windows-msvc` preset clean configure，完成Windows分支相关Debug/Release build与CTest，核对Ninja Multi-Config、MSVC x64、utf8proc/SDL3 BUNDLED、Win32 text input/clipboard、DirectWrite system font discovery和D3D12/DXIL来源，保存独立Windows build evidence
- [ ] 9.2 增加无截图Windows Input acceptance runner，在1.0/1.25/1.5/2.0 acceptance scale启动真实窗口并保存stdout diagnostics/exit code；自动覆盖Latin、selection、clipboard、undo/redo、Theme/status与caret idle，runner不得自动勾选人工IME/视觉任务
- [ ] 9.3 由用户在Windows真实窗口直接验证系统中文IME composition/candidate/commit/cancel、候选窗跟随caret、Ctrl shortcuts、pointer drag/double-click、Default/error focus、CJK/Latin/emoji、四档DPI无裁切和等待后无持续submit；记录用户确认、driver/font/scale/diagnostics路径，不要求截图
- [ ] 9.4 运行Windows evidence passed contract、平台分支tests、dependency/shader/lock/license/cache、OpenSpec strict validate与`git diff --check`；以英文 `test: validate Windows text input` 提交并推送Windows evidence，核对remote SHA，不修改Linux清单

## 10. Linux 专属验收

- [ ] 10.1 使用`linux-gcc` clean configure和`linux-clang` Debug build，完成Linux分支相关CTest，核对Ninja Multi-Config、标准C++20、utf8proc/SDL3 BUNDLED、Wayland text input/clipboard、Fontconfig和Vulkan/SPIR-V来源，保存独立Linux build evidence
- [ ] 10.2 在原生Linux Wayland真实窗口以至少两档实际display scale验证系统IME composition/candidate/commit/cancel、候选窗位置、Ctrl shortcuts、selection/clipboard/undo/redo、Theme/status与caret idle；保存compositor/IME/driver/font/scale/diagnostics/exit code，不以X11代替且不要求截图
- [ ] 10.3 人工核对CJK/Latin/emoji、prefix/suffix、placeholder/selection/caret/composition underline、Default/error focus、clip/scroll和DPI无裁切；运行Linux evidence passed contract、dependency/shader/lock/license/Fontconfig/cache检查
- [ ] 10.4 运行Linux平台分支tests、OpenSpec strict validate与`git diff --check`；以英文 `test: validate Linux text input` 提交并推送Linux evidence，核对remote SHA，不修改Windows清单

## 11. Change 收口

- [ ] 11.1 在准备archive时运行`openspec doctor --json`、`openspec validate --all --strict --no-interactive`、最终受影响CTest/benchmark、public dependency、lock/license/cache、`git diff --check`、remote SHA与clean worktree检查；确认平台通用、Windows与Linux checkbox/evidence各自真实完成，README、AGENTS、architecture、generated docs与OpenSpec职责未混写，本项不替代任何平台验收
