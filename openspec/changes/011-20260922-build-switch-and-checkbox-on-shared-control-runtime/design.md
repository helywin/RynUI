# Design

## Context

见 [proposal.md](proposal.md) 的动机。当前 `ButtonComponentHost` 同时持有 `TextComponentHost`、`InteractionRegistry`、`HitTestSnapshot`、`ComponentSceneComposer`、`ButtonSceneService`、`FocusManager`、`PointerRouter` 与 `AnimationRuntime`；`InputComponentHost` 引用它并通过 auxiliary 同步。`Input` 的编辑 store、clipboard 与唯一平台 text-input session 仍有独立所有权。Button 的 pointer capture/press/activate 代码与 FocusManager 的键盘语义已通过既有合同，不应在提取时改变。现有 Theme/Token 和 RoundedEffect 服务可供新控件使用；Ant Design 参考固定为 6.5.0，不以在线最新版替换。

## Goals / Non-Goals

**Goals:**

- 让窗口级输入、scene、动画与同步阶段拥有与 Button 无关的内部 owner；Button、Input、Switch、Checkbox 成为对等 consumer。
- 把 pointer 按压手势的安全收口抽成小型内部行为，控件自行声明 keyboard 与 activation policy。
- 用 Switch/Checkbox 的实现和混合控件测试证明新增控件复用公共机制、无无关 remount/shape/upload。

**Non-Goals:**

- 不发布公共 `Control` 基类、通用视觉 `Modifier`、通用表单框架或组件代码生成器。
- 不把单行 `TextEditorState` 泛化为多行，也不在本 change 实现 Search、Password、InputNumber。
- 不把 Button focus ring、Input active shadow 或各选中控件的状态视觉合并为一个无差别皮肤。
- 不把 change 008 的滚动性能问题或未完成 Windows/Linux 验收并入本 change。

## Decisions

### 1. 渐进提取窗口级组件服务

新建 internal window-owned `ComponentRuntimeServices`（最终名称可在实现时调整），拥有现有共享 Text、interaction、HitTest、scene、focus、pointer、animation 设施和统一的 frame synchronization 顺序。Button、Input 与新控件取得受限引用，保留自身 mounted state、Props 连接、Token 解析、布局和视觉层。先以现有 Button/Input 适配器迁移，运行全部旧合同，再挂载新控件；不在同一步重写 Input 的编辑算法。`TextInputSessionHost` 仍在单一窗口拥有唯一平台会话的服务中，由 Input 及后续文本类 consumer 共享；不得为每个控件类型实例化第二个 session host。

备选是让所有新控件继续依赖 `ButtonComponentHost`，初期代码最少但名称、生命周期和同步阶段将持续耦合；另一个备选是一次性建立通用组件框架，范围过大且会模糊 typed Props/slots。选择只提取已经被两个 consumer 使用的设施。

### 2. 按压行为与激活策略分层

内部 `PressableBehavior` 只处理 primary pointer identity、capture、pressed、inside release、cancel/disable/destroy 以及回调前收口。它返回一次性 activation intent，不直接调用 `onClick` 或更改 checked。Button 保持现有 Enter/Space 与 loading 规则；Switch/Checkbox 使用 Space toggle，checked 与 indeterminate 的状态更新交由各自 host。FocusManager 继续决定 focus-visible 与键盘输入 route，行为层不复制焦点树。为回调自毁、stale generation、多 pointer 和窗口失焦保留独立回归。

备选是复制 Button 的 `handle_pointer` 到每个控件，短期看快，但取消、重入和多 pointer 修复必须反复传播；把所有键盘规则塞进 Pressable 也会误用于 Input。因此只共享手势生命周期，不共享控件语义。

### 3. 选中状态与场景保持控件专属

Switch/Checkbox 各有 typed Props 和 controlled/uncontrolled 模式；mode 在 mount 时固定。受控交互只报告 `onChange(!checked)`，显示以 authoritative Prop 为准；非受控交互在本 generation 内更新本地值。Checkbox indeterminate 只作为独立展示 Prop，不由 toggle 隐式清除。Checkbox label 使用 typed content slot，Text 继承语义前景；Switch 首批不开放任意内部 children。轨道/滑块、方框/勾号、focus effect 使用现有 retained Quad/RoundedEffect/Text 设施，但拥有各自固定 scene topology 和 Token resolver。与 Button 共用 `ControlSize` 只在锁定参考允许的尺寸集合内映射，非法尺寸在 Props 校验时拒绝。

备选是以 Button 包装两种控件，能快速看到画面，却会继承错误的 focus/pressed/disabled 与尺寸语义，并复制 Button 视觉。选择共享底层 scene 与行为机制，而非嵌套 Button。

### 4. 以真实第二、第三 consumer 驱动小型提取

先锁定 Switch/Checkbox 在 Ant Design 6.5.0 的 source path、状态矩阵、Token identity、几何与尺寸，再补 typed Component Token 和测试。仅当 Button 与新控件出现相同动画 target/retarget/dispose 样板时提取 typed transition helper；不预先设计公开动画 DSL。所有颜色/效果变化明确 dirty domain，禁止用方便的全树 rebuild 替代局部更新。

### 5. 分平台验证边界

使用 `CMakePresets.json` 的正式 `Ninja Multi-Config` preset。平台通用 API、状态机、scene、Token、无网络/依赖/性能合同在一个受支持平台完成一次；Windows 使用 MSVC x64、Win32/D3D12/DXIL 实窗，Linux 分别使用 GCC/Clang 构建并以原生 Wayland/Vulkan/SPIR-V 实窗验收各自平台行为。两个平台的 checkbox/evidence 独立，不能以 Windows 通过代表 Linux 已完成。新平台视觉证据需要实际 scale、系统字体、driver/shader、截图与正常退出记录。

## Risks / Trade-offs

- [宿主提取引起既有 Button/Input 行为回归] → 先做行为等价适配，沿用原有 unit/headless/benchmark，再增加混合控件生命周期测试；不修改它们的公开 API。
- [共享行为过度抽象，控件语义被迫趋同] → Pressable 只输出手势结果，键盘 policy、checked、loading、focus 外观仍由 consumer 定义。
- [受控回调与同步销毁造成 stale access] → 在回调前释放 capture/pressed，回调后重新校验 generation；对自毁和 Signal 回写分别测试。
- [Token 或 scene 新增导致全树更新] → 锁定每个状态的 dirty domain、scene identity、upload 范围与 idle benchmark，超出预算视为未通过。
- [一次性实施过大] → 以共享服务、按压行为、Switch、Checkbox、各平台验收为独立提交边界；每阶段仅提交当阶段相关文件。

## Migration Plan

内部服务迁移先保持 Button/Input 对外行为与 ABI 不变；公共头只增加 Switch/Checkbox。若新控件阶段失败，保留已验证的内部提取提交并回退未验证的新控件阶段，而不修改旧 Props 合同。change 008 的部分人工截图与待验收清单保持原样。
