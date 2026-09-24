# Design

## Context

见 [proposal.md](proposal.md) 的动机。change 013 已将 Text、interaction、hit-test、scene、surface、focus、pointer、animation、编辑设施与帧同步迁入内部 `WindowComponentServices`，并提供 `PressableBehavior`；Button/Input 在 Windows MSVC 下的回归已记录，013 的原生 Linux Wayland 项仍未完成。011 以这些实现为前置，新增控件必须使用同一窗口服务，且各自保留键盘、checked 与视觉策略。现有 Theme/Token 和 RoundedEffect 服务可供新控件使用；Ant Design 参考固定为 change 012 核实的 6.6.5，不以在线最新版替换。

## Goals / Non-Goals

**Goals:**

- Switch/Checkbox 与 Button/Input 使用 change 013 的同一窗口级输入、scene、动画与同步 owner。
- Switch/Checkbox 复用 `PressableBehavior` 的 pointer 手势收口，控件自行声明 keyboard 与 activation policy。
- 用 Switch/Checkbox 的实现和混合控件测试证明新增控件复用公共机制、无无关 remount/shape/upload。

**Non-Goals:**

- 不发布公共 `Control` 基类、通用视觉 `Modifier`、通用表单框架或组件代码生成器。
- 不把单行 `TextEditorState` 泛化为多行，也不在本 change 实现 Search、Password、InputNumber。
- 不把 Button focus ring、Input active shadow 或各选中控件的状态视觉合并为一个无差别皮肤。
- 不把 change 008 的滚动性能问题或未完成 Windows/Linux 验收并入本 change。

## Decisions

### 1. 复用窗口级组件服务

Switch/Checkbox 取得 `WindowComponentServices` 受限引用并注册为同步参与者，保留自身 mounted state、Props 连接、Token 解析、布局和视觉层。服务已由 013 实现共同的 mount/destroy、Text、interaction、HitTest、scene、focus、pointer、animation 与帧同步顺序；011 只在真实新 consumer 需要时扩展窄接口。`WindowTextEditServices` 保持单一平台会话，选中型控件不创建编辑会话。

备选是让新控件依赖 `ButtonComponentHost` 或各自新建一套宿主；两者都会重复交互与帧同步逻辑，因此直接复用 013 的窗口服务，不建立公开通用组件框架。

### 2. 按压行为与激活策略分层

013 的内部 `PressableBehavior` 已处理 primary pointer identity、capture、pressed、inside release、cancel/disable/destroy 以及回调前收口，并返回一次性 activation intent。Switch/Checkbox 使用该行为输出，但自行执行 Space toggle、checked 与 indeterminate 的状态更新；Button 保持原 Enter/Space 与 loading 规则。FocusManager 继续决定 focus-visible 与键盘输入 route。新控件仍须覆盖回调自毁、stale generation、多 pointer 和窗口失焦的自身集成回归。

备选是复制 Button 的 `handle_pointer` 到每个控件，短期看快，但取消、重入和多 pointer 修复必须反复传播；把所有键盘规则塞进 Pressable 也会误用于 Input。因此只共享手势生命周期，不共享控件语义。

### 3. 选中状态与场景保持控件专属

Switch/Checkbox 各有 typed Props 和 controlled/uncontrolled 模式；mode 在 mount 时固定。受控交互只报告 `onChange(!checked)`，显示以 authoritative Prop 为准；非受控交互在本 generation 内更新本地值。Checkbox indeterminate 只作为独立展示 Prop，不由 toggle 隐式清除；其居中方块边长按 6.6.5 来源的 `fontSizeLG / 2` 派生，不画成横线。Checkbox label 使用 typed content slot，Text 继承语义前景；Switch 首批不开放任意内部 children。轨道/滑块、方框/勾号、focus effect 使用现有 retained Quad/RoundedEffect/Text 设施，但拥有各自固定 scene topology 和 Token resolver。Switch 若复用 `ControlSize`，只接受 Middle/Small 并拒绝 Large；Checkbox 来源无 size Prop，保持单一来源尺寸而不暴露伪造的尺寸 API。

备选是以 Button 包装两种控件，能快速看到画面，却会继承错误的 focus/pressed/disabled 与尺寸语义，并复制 Button 视觉。选择共享底层 scene 与行为机制，而非嵌套 Button。

### 4. 以真实第二、第三 consumer 驱动小型提取

先锁定 Switch/Checkbox 在 Ant Design 6.6.5 tag `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 source path、逐文件 SHA256、状态矩阵、Token identity、几何与尺寸，再补 typed Component Token 和测试。关键来源为 `components/switch/index.en-US.md`、`components/switch/style/index.ts`、`components/checkbox/index.en-US.md`、`components/checkbox/style/index.ts`，完整对照见 change 012 的 `evidence/source-diff.json`。仅当 Button 与新控件出现相同动画 target/retarget/dispose 样板时提取 typed transition helper；不预先设计公开动画 DSL。所有颜色/效果变化明确 dirty domain，禁止用方便的全树 rebuild 替代局部更新。

### 5. 分平台验证边界

使用 `CMakePresets.json` 的正式 `Ninja Multi-Config` preset。平台通用 API、状态机、scene、Token、无网络/依赖/性能合同在一个受支持平台完成一次；Windows 使用 MSVC x64、Win32/D3D12/DXIL 实窗，Linux 分别使用 GCC/Clang 构建并以原生 Wayland/Vulkan/SPIR-V 实窗验收各自平台行为。两个平台的 checkbox/evidence 独立，不能以 Windows 通过代表 Linux 已完成。新平台视觉证据需要实际 scale、系统字体、driver/shader、截图与正常退出记录。

## Risks / Trade-offs

- [新控件使用共享服务引起既有 Button/Input 行为回归] → 沿用 013 的 unit/headless/benchmark，并增加四控件混合生命周期测试；不修改既有公开 API。
- [共享行为过度抽象，控件语义被迫趋同] → Pressable 只输出手势结果，键盘 policy、checked、loading、focus 外观仍由 consumer 定义。
- [受控回调与同步销毁造成 stale access] → 在回调前释放 capture/pressed，回调后重新校验 generation；对自毁和 Signal 回写分别测试。
- [Token 或 scene 新增导致全树更新] → 锁定每个状态的 dirty domain、scene identity、upload 范围与 idle benchmark，超出预算视为未通过。
- [一次性实施过大] → 共享服务和按压提取已由 013 独立提交；011 以来源合同、Switch、Checkbox、各平台验收为独立提交边界。

## Migration Plan

沿用 013 的内部服务，不重复迁移 Button/Input；公共头只增加 Switch/Checkbox。若新控件阶段失败，保留 013 已验证的内部提取提交并回退未验证的新控件阶段，而不修改旧 Props 合同。013 的 Linux 项及 change 008 的人工截图待验收清单保持各自原状态。
