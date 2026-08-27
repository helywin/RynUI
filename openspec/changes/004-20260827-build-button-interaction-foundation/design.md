## Context

见 `proposal.md` 的动机与范围。003 已提供 generation-checked `ComponentHost`、每个组件一个 root Node、内部 `BoxLayout`/`FlexLayout`、intrinsic Text measure、共享 GlyphAtlas/Text scene service、公开 `Prop<T>`/`LayoutStyle`/`ryn::Text`，但 SDL event 目前只汇总为 quit/frame-request bool。`Node` 已有 committed bounds、translation 与 `DirtyFlags::HitTest`，却没有交互注册表、事件 route 或 focus owner。

本 change 横跨 platform、Runtime、Layout、Component、Scene 和 renderer。输入必须只在 Window owner thread 派发；回调可以同步写 Signal、销毁 Button 或父 Scope；hit target 必须对应上一帧已提交的 geometry 与 paint order。Button 视觉参照锁定的 Ant Design 6.5.0 源码合同，不复制 React、DOM 或 CSS-in-JS 实现。

## Goals / Non-Goals

**Goals:**

- 建立可由 synthetic events 完整测试的平台无关 Pointer/Keyboard/Window focus 输入值。
- 让 HitTest、Capture/Target/Bubble、hover、pointer capture 和 Focus 共享 generation-checked interaction identity。
- 让公开 Button 复用现有 ComponentHost、Text shaping/GlyphAtlas、LayoutStyle 与按需帧系统。
- 保持 Button scene、Text scene、HitTest 顺序来自同一个 component paint traversal，并保留逐组件局部 dirty range。
- 在注册容量稳定后，使 pointer steady-state route 不分配；所有状态稳定且没有 animation task 时停止 submit。

**Non-Goals:**

- 不在本 change 发布 public Application/Window、原始事件 listener、事件对象或 imperative focus handle。
- 不为首批 Button 增加 Dashed/Text/Link/Danger/Ghost/Icon/Block/ButtonGroup、任意视觉 override 或 click wave。
- 不实现 Scroll/Clip stack、DragDrop、手势识别、IME、Accessibility、多窗口 focus transfer 或通用动画 scheduler。
- 不把内部 Button content 布局直接提升为公开 Flex/Space API，也不承诺复杂富内容和 baseline layout 已完成。

## Decisions

### 1. SDL event 在 platform adapter 内归一化并按原顺序交给 Runtime

`src/platform/sdl/` 将 SDL mouse、finger、keyboard、window focus 与 cancel 事件转换为 RynUI 自有 `PlatformInputEvent` tagged value。mouse 坐标直接使用 window logical coordinates；归一化 touch 坐标使用事件对应窗口的 logical content size 转换。mouse 使用保留 pointer identity，touch identity 由当前 Window 内的 finger identity 映射，Window Runtime 不共享 pointer state。

现有 `PlatformEvents` 保留 quit/frame-request 摘要，并增加拥有容量的 input batch。batch 在 event pump 每轮复用 storage；容量增长允许分配，容量稳定后的 move/down/up/key 路径不再分配。不得合并 down/up/key/cancel；连续 move 可在尚未派发前按同一 pointer identity 保留最后位置，但必须保持它与按键边界的相对顺序。

Runtime 在创建 Window 的线程同步消费 batch，随后进入既有 reactive/layout/scene/frame pipeline。输入命中上一帧 committed layout snapshot：这是用户当前看到的几何，而不是本轮 callback 尚未提交的新状态。非 owner thread 调用 dispatch fail-fast。

备选方案是把 `SDL_Event` 传到 Component，或先公开通用 `PointerEvent` API。前者破坏 SDL 隔离，后者会在 capture、坐标、keyboard 与多窗口合同尚未稳定时固化 public API，因此均不采用。

### 2. Interaction identity 与 Node/Component generation 绑定

Runtime 增加单 Window `InteractionRegistry`。每个注册记录包含自己的 slot+generation、所属 ComponentId/NodeId、交互父记录、声明/paint order、eligible/focusable flags、有效 hit bounds/clip 和内部 handlers。Button 注册自己的 root；Text/glyph 默认是纯视觉节点，将命中归属给 Button 交互祖先。

Layout/geometry commit 后只同步 `DirtyFlags::HitTest` 对应记录。第一版使用按 paint order 排列的 dense records 反向扫描；规模较小且行为确定，避免过早引入空间树。命中先检查 window clip，再检查祖先 effective clip、translation 后 bounds 与 eligible，选择最后绘制的最深 target。后续可用 R-tree/BVH 替换索引而不改变 route 合同。

Interaction registration 的 cleanup 放入 Component resource cleanup。销毁先停止 Scope/Prop，再从 registry 移除 focus/capture/hover 与 scene fragment，最后销毁 Node 和推进 generation。任何 callback 前重新校验 interaction、Component 与 Node 三个 identity。

备选方案是只保存裸 NodeId 或每次事件遍历 Node 树。裸 NodeId 不能表达 handler 生命周期；全树遍历会把纯视觉节点和交互语义耦合，也难以证明 Dirty 最小范围，因此不采用。

### 3. Paint traversal 同时生成 OrderedScene 与 HitTest order

ComponentHost 增加内部只读 paint traversal：每个 component 可以注册 `before-children` 与 `after-children` scene fragment。Button 的 focus/background/border fragment 位于 children 之前，Text glyph fragment 位于 Text component 自身；深度优先遍历先完整输出前一个 sibling，再输出后一个 sibling。`graphics::OrderedScene` 已支持 Quad/Glyph command 交错，renderer 继续按命令切换 pipeline。

同一次 traversal 给 InteractionRegistry 写入 paint order，因此重叠 sibling 的视觉最上层与 HitTest target 不会由两套排序规则推导。仅 Material/Geometry 更新不重建命令序列；component 创建、销毁或结构顺序变化才重建对应 command list。当前 004 没有公开 `If`/`For`，但显式 destroy 仍覆盖 compact/remap。

备选方案是所有 Button Quad 先画、所有 Text 后画。它在不重叠示例里可见结果正确，但 sibling 重叠时会让早期 Button 的 Text 穿过后期 Button 背景，并与 HitTest order 漂移，因此不采用。

### 4. Pointer router 使用 route snapshot，但每个 handler 前重检 generation

每个 pointer identity 保存 position、buttons、hover path、optional capture 和 press origin。事件首先按实际位置刷新 hover path；若存在 capture，move/up/cancel 的 dispatch target 使用 capture identity，否则使用 HitTest target。route 在事件开始时从 target 沿 interaction parent 链构建到预留 scratch storage，并按 Capture、Target、Bubble 执行。

handler 可以停止传播。handler 也可以同步 disable、loading、destroy 或 dispose；router 在下一 handler 前验证记录和 generation，失效则跳过剩余 stale route 并执行统一 cancel cleanup。Button down handler 在确认主 pointer/eligible 后设置 pressed 与 capture；up 先释放 capture/pressed，再检查同一 pointer 的 press origin 与当前实际 hit，最后调用 click callback。这样 self-destroy 或 callback exception 都不会留下 capture。

备选方案是在 up 后保留 capture 到 callback 返回，或保存 handler 引用直接遍历。前者使 self-destroy cleanup 复杂，后者在 vector compact/slot reuse 后悬空，因此不采用。

### 5. Focus manager 与 pointer router 共享资格记录

每个 Window Runtime 拥有一个 `FocusManager`，保存 focused interaction identity、input modality、window-active 和 keyboard press state。focus order 从 InteractionRegistry 的 eligible+focusable records 按稳定声明顺序生成；`Tab`/`Shift+Tab` 在首尾循环。disabled 从 focus order 移除；loading Button 仍可聚焦，但 activation gate 返回 false。

pointer down 请求 focus 并把 modality 设为 pointer，因此不显示 ring。`Tab`/`Shift+Tab` 把 modality 设为 keyboard 并显示 focus-visible。window blur 保留仍有效的 focused identity以便恢复，但隐藏 ring并取消 Space/Pointer press；destroy/disabled 立即清除 stale focus。`Enter` 只在非 repeat key down 激活一次；`Space` key down 设置 pressed，匹配 key up 先收口再 click。

备选方案是让 Button 各自监听全局 keyboard 或用一个 bool 表示 focus。前者无法统一 tab order，后者不能区分 pointer focus 与 focus-visible，也不能防止 slot reuse，因此不采用。

### 6. 首批公开 Button API 只固定可完整兑现的子集

公开形态为：

```cpp
ryn::Button(
    ryn::ButtonProps{}
        .type(ryn::ButtonType::Primary)
        .size(ryn::ControlSize::Middle)
        .disabled(disabled)
        .loading(loading)
        .onClick([=] { submit(); })
        .layout(ryn::LayoutStyle{}.width(ryn::dp(160))),
    [] { ryn::Text(u8"确定"); });
```

`type`、`size`、`disabled`、`loading` 使用 `Prop<T>`；`onClick` 保存 owning `std::function<void()>`，不暴露尚未稳定的 raw input event。`ButtonContent` 使用独立 slot tag，不能误传其他组件专属 slot。Default/Primary 与 Small/Middle/Large 在本 change 完整实现；新增 enum value 可由后续 change source-compatible 扩展。

Button root 使用内部 horizontal content layout：测量直接 children 的 intrinsic size，加 Button token padding/border并居中 place。实现所需 cross-axis alignment 作为内部 Layout model 能力加入，不提前发布 public Flex。`LayoutStyle` 仍只编译到 Button root 的 external style。

备选方案是让 `ButtonProps` 接受 String label 或公开任意 background/border。String label 会产生第二套 content 生命周期并阻碍 icon/复合 content；任意视觉入口与 Component Token 冲突，因此不采用。

### 7. Text 通过内部语义前景 context 继承 Button token

Component build context 增加不公开的 semantic foreground context。Button mount 创建一个拥有生命周期的前景 Signal/Binding，并在挂载 content slot 时压入；TextProps 未显式调用 `.tone(...)` 时订阅该 context，显式 tone 则继续使用 Text alias。Button type/hover/pressed/disabled/loading 更新前景只改 Text Material，不 shape 或 measure。

为区分“默认值”与“显式 Primary”，TextProps 内部把 tone 存储调整为 optional Prop；Button 外没有 context 时仍解析为现有 Primary alias，因此公开源代码和默认视觉保持兼容。child Text subscription 随 child Scope 先停止，Button context 随 parent state 后销毁。

备选方案是 Button 直接修改后代 Glyph range 或复制一套 label renderer。前者绕过组件所有权，后者复制 shaping/atlas 并使 slot 名存实亡，因此不采用。

### 8. Button scene 固定 visual layers 并使用局部 range 更新

Button scene service 为每个 Button 保存 generation-checked fixed visual range：focus ring、border、background 与静态 loading indicator 所需 instances。不存在的状态将对应 instance opacity 设为零，因此 hover/focus/disabled 不改变 command topology。边框与 focus ring由同心 rounded Quad layers 表达，Button hit bounds 始终是 root bounds，不包含 ring。

Quad instance store 扩展为与 Glyph store 一致的 append/replace/compact、Material/Geometry dirty ranges。Button 销毁只 compact 后续 Button ranges 并更新 fragment mapping；GPU buffer capacity 可复用，上传仅覆盖移动或变化范围。loading indicator 在首批实现为不依赖持续 timer 的静态状态标记；loading 出现/消失允许局部 Measure/Layout 与 geometry 更新，完整 spinner animation 留给 animation change。

备选方案是为每个 Button 建独立 GPU buffer/draw pass，或每次 hover 重建所有 Quads。两者都会放大资源和 submit/upload 成本，因此不采用。

### 9. Default Theme snapshot 锁定 Button Component Token，不发布 Theme API

内部 snapshot 增加 Button token，基线来自 Ant Design 6.5.0：control height 24/32/40、正文 14 logical pixels、base radius 6、Primary `#1677ff` 及 hover/active 色阶、Default container/text/border、disabled background/text/border、focus-visible 与 loading opacity。padding/radius/foreground 等派生值在一个 token contract test 中集中锁定。

参考边界：

- [Button API 与 loading/disabled click gate](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/Button.tsx)
- [Button 尺寸与共享状态样式](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/index.ts)
- [Button Component Token 派生](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/token.ts)
- [Button variant 状态颜色](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/button/style/variant.ts)
- [Ant Design 6.5.0 Seed Token](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/theme/themes/seed.ts)

RynUI 只复制可测试的设计值与状态语义，不引入 React/CSS 代码，也不开放 runtime Theme override。完整 Seed->Map->Alias->Component 算法仍由独立 change 实现。

### 10. 输入、响应、布局和提交保持确定帧阶段

单轮顺序固定为：platform batch -> pointer/keyboard dispatch -> reactive binding flush -> Measure/Layout -> HitTest snapshot sync -> Button/Text scene sync -> dirty range upload -> ordered draw -> Effect。pointer 命中使用进入本轮前的 committed snapshot，回调产生的新布局在同轮后半段提交；不会让同一事件先命中旧树又在传播中切换到新树。

状态变化通过现有 frame request 合并。hover/pressed/focus只标记 Button material，size/loading indicator标记目标 Measure/Layout/Geometry/HitTest，外部 translation标记 Geometry/HitTest。无 dirty、无 input且无 animation deadline时继续阻塞等待事件。

### 11. 构建与验收严格按平台拆分

不增加第三方 dependency 或 CMake 模式。正式 build 继续使用 Ninja Multi-Config：Linux 分别运行 `linux-gcc` 与 `linux-clang`，Windows 只使用 `windows-msvc`。`tasks.md` 把平台无关自动测试、Linux build/CTest/真实窗口/evidence、Windows build/CTest/真实窗口/evidence 放在独立章节和 checkbox；任一平台完成后可单独提交验收证据，另一平台未执行不会回退已完成项。

## Risks / Trade-offs

- **[dense HitTest 扫描在大列表中是 O(n)]** → 首批增加诊断与 1k records benchmark；保持 registry/query 接口可替换，VirtualList 前再评估空间索引。
- **[callback 同步销毁导致 route/scene compact]** → route 保存 identity 不保存引用；每个 handler 前重检 generation，click 前先释放 capture/pressed，compact 后重建 fragment mapping。
- **[touch 与 mouse 可能由平台生成兼容事件而重复激活]** → platform adapter 按 SDL source/identity 去重同一物理序列，router 的 press origin 只接受一个 primary identity。
- **[Button content 继承颜色改变 TextProps 内部表示]** → 保持 public API 不变，以显式 tone contract、Button 外默认 Primary 和生命周期测试覆盖兼容性。
- **[多层 rounded Quad 不能完全等价 CSS outline/shadow]** → 锁定 logical geometry、颜色与状态层级并做真实窗口截图；阴影、wave 与高级动画不在本 change 宣称完成。
- **[loading 静态 indicator 与 Ant Design 动画不同]** → 保留 loading 的视觉可辨识、opacity 和 click gate，明确完整 animation 后置，不以本阶段截图宣称动画一致。
- **[Windows 与 Linux 输入细节不同]** → synthetic contract覆盖共同语义，各平台真实窗口清单独立保存 driver、display scale、事件计数和截图，不用一台机器结果外推另一平台。

## Migration Plan

1. 增加平台无关 input values、interaction registry/router 与 Focus manager，不改变现有示例行为。
2. 接入 SDL event adapter，并用 fake platform/synthetic batches保持现有 quit/resize/frame-request contract。
3. 增加 component paint traversal、Quad range store 与 Button scene service，再接入 Default Theme Button token。
4. 发布 Button typed API，接通 content foreground context、Layout、Pointer、Focus 与 click。
5. 将交互示例迁移为公开 Button DSL，分别完成 Linux 与 Windows 独立验收清单。

任一实现阶段可通过回退该阶段独立 commit 恢复到上一阶段；公开 Button header 只有在 public API contract、生命周期和 headless interaction tests 同阶段通过后才提交。003 的 Text API 与已有示例不依赖 Button，回退 004 不需要迁移用户数据。
