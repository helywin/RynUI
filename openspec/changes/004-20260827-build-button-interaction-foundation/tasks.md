## 1. 平台无关输入值与 SDL adapter

- [x] 1.1 定义 RynUI 自有 pointer、keyboard、window focus/cancel tagged values 与可复用 input batch，保留事件顺序和 logical coordinates；通过 value/queue tests 覆盖 mouse identity、touch identity、key repeat、modifier、batch capacity reuse 与非法值
- [x] 1.2 将 SDL mouse/finger/key/window event 映射封装在 platform adapter 内，完成 touch logical coordinate 转换与兼容 mouse 去重；通过 fake SDL-shaped source tests 覆盖 down/move/up/cancel、focus lost/gained、quit/resize 和现有 frame-request 回归
- [x] 1.3 接入 owner-thread event pump 与诊断计数，通过错误线程、稳定容量 allocation counter、连续 move 边界和 public forbidden-include tests 证明输入不泄漏 SDL3 且 steady-state batch 不分配
- [x] 1.4 运行 platform input、platform lifecycle、frame scheduler 与 public dependency 自动测试和 `git diff --check`；以英文 `feat: normalize platform input events` 提交并推送本阶段相关文件，核对 remote SHA

## 2. Interaction registry 与 HitTest

- [x] 2.1 实现 slot+generation 的 interaction identity 与 owner-thread registry，记录 ComponentId/NodeId、交互 parent、eligible/focusable、handler 和 cleanup；通过 create/find/remove/reuse、错误线程和 Node/Component stale identity tests 验证生命周期
- [x] 2.2 建立由 committed bounds、translation、window/effective clip 与 component paint traversal 同步的 HitTest snapshot；通过嵌套、margin、translation、clip、纯视觉 child 委托和无当前 layout tests 验证 logical geometry
- [x] 2.3 实现按 paint order 反向选择最深 eligible target，通过重叠 sibling、祖先/后代、disabled、边界点与 slot reuse tests 证明 target 与绘制顺序一致
- [x] 2.4 将 bounds/translation/clip/资格/顺序接入 `DirtyFlags::HitTest` 最小同步，增加 HitTest/refresh/stale-skip 计数与 1k records benchmark；证明 Material/Text-only 更新不刷新无关记录且稳定 query 不分配
- [x] 2.5 运行 interaction registry、Node、Layout、Dirty 与 benchmark contract 自动测试和 `git diff --check`；以英文 `feat: add retained hit testing` 提交并推送本阶段相关文件，核对 remote SHA

## 3. Pointer 路由、hover 与 capture

- [x] 3.1 实现预留 route scratch 上的 Capture、Target、Bubble 派发与 stop propagation，通过完整顺序、各阶段停止、空 route、嵌套 target 和同一 handler 不重复执行 tests 验证传播合同
- [x] 3.2 实现 per-pointer position/buttons/hover path/capture/press origin，实际 HitTest 决定 hover、capture target 接收 move/up/cancel；通过 enter/leave、拖出、拖回、不同 pointer identity 和非主按键 tests 验证隔离
- [x] 3.3 在每个 handler 前重检 interaction/Component/Node generation，并在 up、cancel、窗口失焦、disable、destroy、Scope dispose 与 callback exception 前后统一释放 capture；通过 self-destroy、ancestor destroy、slot reuse 和 reentrant dispatch fail-fast tests 验证无 stale access
- [x] 3.4 接通 pointer route/capture/cancel/stale-skip 诊断与 frame request，使用 controlled clock/allocation tests 证明 Material 状态变化只请求必要帧、稳态 move 不分配且 idle 不持续 submit
- [x] 3.5 运行 Pointer、HitTest、Component lifecycle 与 frame scheduler 自动测试和 `git diff --check`；以英文 `feat: add pointer event routing` 提交并推送本阶段相关文件，核对 remote SHA

## 4. Focus manager 与键盘激活

- [x] 4.1 实现单 Window generation-checked focus identity 与按 eligible 声明顺序生成的 focus order，通过 `Tab`/`Shift+Tab`、首尾循环、空列表、disabled 跳过、loading 保留和动态资格 tests 验证遍历
- [x] 4.2 实现 keyboard/pointer modality、focused/focus-visible 与 window active 状态，通过 pointer focus、Tab focus、window lost/gained、focused target disable/destroy 和 slot reuse tests 验证 ring 资格与恢复
- [x] 4.3 实现 `Enter` 非 repeat key-down 单次激活以及 `Space` down pressed/up click，通过 repeat、mismatched key、失焦、disable/loading、destroy、callback mutation 和 keyboard/pointer 去重 tests 验证 click gate
- [x] 4.4 连接 Focus manager 与 Pointer router 的资格、cancel 和诊断路径，通过 owner-thread、嵌套 route、Scope dispose、steady-state allocation 和 idle frame tests 验证共享 lifecycle
- [x] 4.5 运行 Focus、Keyboard、Pointer、Component lifecycle 与 frame scheduler 自动测试和 `git diff --check`；以英文 `feat: add focus and keyboard activation` 提交并推送本阶段相关文件，核对 remote SHA

## 5. Component paint traversal 与 Button scene 数据

- [x] 5.1 为 ComponentHost 增加 generation-checked `before-children`/`after-children` scene fragment 注册和深度优先 paint traversal，通过 nested/sibling/destroy/reuse tests 证明 Quad/Glyph command 与声明顺序稳定
- [x] 5.2 将同一 paint traversal 的 interaction order 接入 HitTest snapshot，通过重叠 Button-like fragment、Text child 和结构销毁 tests 证明最上层视觉与命中 target 始终一致
- [x] 5.3 扩展 Quad instance store 的 append/replace/compact、Material/Geometry dirty ranges 和固定 Button visual range，覆盖 focus ring、border、background 与静态 loading indicator；通过 sparse update、range remap、opacity-hidden layer 和字节边界 tests 验证不扩大更新
- [x] 5.4 实现 generation-checked Button scene service 与 ordered fragment，接通可复用 GPU buffer 的局部 upload/draw count；通过多个 Button 创建/销毁/复用、Quad/Glyph 交错、buffer capacity 和 renderer failure tests 验证资源收口
- [x] 5.5 运行 Component traversal、OrderedScene、Quad store/Button scene 与 renderer 自动测试和 `git diff --check`；以英文 `feat: add component scene composition` 提交并推送本阶段相关文件，核对 remote SHA

## 6. Button token、content context 与内部布局

- [x] 6.1 扩展 immutable Default Theme snapshot，集中锁定 Ant Design 6.5.0 Button 的 24/32/40 control height、padding、radius、Default/Primary default/hover/active、disabled、focus-visible 与 loading token；通过 source-reference/token contract test 验证字段和值
- [x] 6.2 为 Component build stack 增加内部 semantic foreground context，并使未显式 tone 的 Text 订阅 context、显式 tone 保持原语义；通过 Button 外默认值、嵌套 context、reactive foreground、异常恢复和 parent/child dispose tests 证明只更新 Glyph Material
- [x] 6.3 扩展内部 horizontal content layout 的 cross-axis/main-axis center 与 token padding/control height，保持 public LayoutStyle 只作用 root external style；通过多 child、空 content、Small/Middle/Large、fixed/min/max width、margin、CJK intrinsic 和 constrained content tests 验证测量放置
- [x] 6.4 为静态 loading indicator 接入局部 content Measure/Layout 和 geometry，证明出现/消失不重挂 content、不重塑未变化 Text、不改变兄弟 identity，并在无 animation task 时恢复 idle
- [x] 6.5 运行 Theme、Text context、Layout、Glyph material 与 component lifecycle 自动测试和 `git diff --check`；以英文 `feat: add button visual foundations` 提交并推送本阶段相关文件，核对 remote SHA

## 7. 公开 ButtonProps 与 ryn::Button

- [ ] 7.1 在公开 headers 实现 `ControlSize`、`ButtonType`、拥有 reactive type/size/disabled/loading、owning `onClick` 与 `LayoutStyle` 的 typed `ButtonProps`，以及 Button 专用 content slot；通过 public API/compile-fail tests 证明未支持 slot、narrow callback、任意视觉字段和内部类型不可用
- [ ] 7.2 实现 Button mount adapter：创建 root/state、连接 Prop、挂载 content context/slot、注册 layout/scene/interaction/focus 与逆序 cleanup；通过首次挂载、多个 Button、无 active Host、错误线程、异常 slot、destroy/reuse 和销毁后 Signal write tests 验证生命周期
- [ ] 7.3 实现 Default/Primary 与 Small/Middle/Large 的 state resolver，使 disabled 优先、loading gate、hover/pressed/focus-visible 和 content foreground 原子同步；通过完整状态矩阵与 reactive transition tests 验证 visual/interaction/layout dirty 分类
- [ ] 7.4 实现 pointer down/capture/up/cancel 与 `Enter`/`Space` 汇入同一 click path，在 callback 前收口 pressed/capture；通过 bounds 内外、非主按键、重复输入、loading 防重复、自毁/父销毁和 callback exception tests 验证每个有效手势只调用一次
- [ ] 7.5 扩展 `rynui.hpp`、public target 和 header isolation/leak checks，通过只依赖 public target 的 consumer 编译/链接证明 Button API 不出现 Runtime、Node、Scene、Font、HarfBuzz、FreeType、SDL3 或 GPU 类型
- [ ] 7.6 运行 Button component、Prop、Layout、Text、Pointer、Focus、scene 与 public API 全部自动测试和 `git diff --check`；以英文 `feat: add public button component` 提交并推送本阶段相关文件，核对 remote SHA

## 8. 平台无关示例与 headless 验收

- [ ] 8.1 新增公开 Button DSL 示例，由内部 application host 挂载 Default/Primary、Small/Middle/Large、disabled/loading 和共享 CJK/Latin Text content；通过 source contract 验证示例不直接构造 interaction、focus、Button scene、Text state 或 SDL event
- [ ] 8.2 为示例增加可观测 click counter 与 reactive type/size/disabled/loading 更新，打印 input/HitTest/route/capture/focus/click/layout/scene/upload/draw/submit/idle 计数；通过 controlled input/clock tests 覆盖 pointer、Tab、Enter、Space、drag-out、focus loss 和 loading 防重复
- [ ] 8.3 增加平台独立 evidence schema/contract，要求 display scale、driver、shader format、退出码、截图路径和全套计数，同时为各平台保留独立 evidence 文件且不允许相互代替
- [ ] 8.4 运行 Button example source contract、headless interaction/frame integration、evidence schema、dependency leak 与 `git diff --check`；以英文 `feat: add interactive button demo` 提交并推送本阶段相关文件，核对 remote SHA

## 9. Linux 验收清单

- [ ] 9.1 使用 `linux-gcc` preset clean configure，完成 Debug/Release build 与完整 CTest，核对 Ninja Multi-Config、标准 C++20、BUNDLED 依赖和 Vulkan/SPIR-V shader 仍来自锁定来源，保存 Linux 构建结果
- [ ] 9.2 使用 `linux-clang` preset 完成 Debug configure/build/CTest，核对 RynUI 自有 input/interaction/focus/Button/scene target 使用 `-std=c++20` 而非 GNU extensions；环境缺 Clang 时保持未完成并记录可复现阻塞
- [ ] 9.3 在 Linux/GCC/Vulkan/SPIR-V 真实窗口运行公开 Button DSL 示例，以 mouse 和 keyboard 实际触发 hover、pressed、drag-out、Tab、Enter、Space、disabled/loading 与正常退出；保存 display scale、截图、driver、shader format、退出码和全套诊断计数
- [ ] 9.4 人工核对 Default/Primary、Small/Middle/Large、CJK/Latin、focus-visible、disabled/loading 层级和 click counter；运行 Linux evidence contract、public dependency、shader/lock/license 与未跟踪依赖检查，不把其他平台结果写入 Linux 证据
- [ ] 9.5 运行 Linux 完整 CTest 与 `git diff --check`；以英文 `test: validate Linux button interactions` 提交并推送本平台验收文件，核对 remote SHA

## 10. Windows 验收清单

- [ ] 10.1 使用 `windows-msvc` preset clean configure，完成 Debug/Release build 与完整 CTest，核对 Ninja Multi-Config、MSVC x64、BUNDLED 依赖和 D3D12/DXIL shader 仍来自锁定来源，保存 Windows 构建结果
- [ ] 10.2 在 Windows/MSVC/D3D12/DXIL 真实窗口运行公开 Button DSL 示例，以 mouse 和 keyboard 实际触发 hover、pressed、drag-out、Tab、Enter、Space、disabled/loading 与正常退出；保存 display scale、截图、driver、shader format、退出码和全套诊断计数
- [ ] 10.3 人工核对 Default/Primary、Small/Middle/Large、CJK/Latin、focus-visible、disabled/loading 层级和 click counter；运行 Windows evidence contract、public dependency、shader/lock/license 与未跟踪依赖检查，不把其他平台结果写入 Windows 证据
- [ ] 10.4 运行 Windows 完整 CTest 与 `git diff --check`；以英文 `test: validate Windows button interactions` 提交并推送本平台验收文件，核对 remote SHA

## 11. Change 收口

- [ ] 11.1 在准备 archive 时运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`，确认所有独立验收条目已完成、worktree 只包含本 change 相关文件且 README、AGENTS、architecture 与 OpenSpec 职责未混写；本项只校验 change 完整性，不替代任何平台验收
