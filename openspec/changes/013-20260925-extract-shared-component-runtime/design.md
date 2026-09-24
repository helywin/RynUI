# Design

## Context

见 [proposal.md](proposal.md)。`ButtonComponentHost` 目前拥有 `TextComponentHost`、`InteractionRegistry`、`HitTestSnapshot`、`ComponentSceneComposer`、`ButtonSceneService`、`FocusManager`、`PointerRouter`、`AnimationRuntime` 和辅助同步列表；`InputComponentHost` 使用的均为这些通用接口，唯独挂载与销毁还借用 Button 宿主入口。Input 的 editor store、session 和 clipboard commands 已有独立类型，但对象仍由 Input 宿主持有。

## Goals / Non-Goals

**Goals:**

- 将已存在的窗口级对象和同步顺序迁入单一内部 owner；Button/Input 各自只保留组件状态、Props、Token、布局与视觉实现。
- 使 Input 的挂载、销毁、帧参与和编辑服务接入不依赖 `ButtonComponentHost` 类型，并保持同窗唯一平台 text-input session。
- 复用 Button 已验证的 pointer press 生命周期，保持既有 Button 激活、焦点、动画和场景表现。

**Non-Goals:**

- 不添加公开 `Control` 基类、组件工厂或视觉 `Modifier`，不提前提取尚无第二个消费者的 material transition helper。
- 不改变单行编辑模型或将 focus ring、Input active shadow 统一成一种外观。
- 不实施 Switch/Checkbox/Search，也不完成 011 的平台验收。

## Decisions

### 1. 窗口服务单一所有权

新增 internal `WindowComponentServices`，由窗口或测试 fixture 在 Button/Input 宿主之前构造，统一拥有 Text、interaction、hit-test、scene、focus、pointer、animation 和现有 retained surface 服务。保留原对象及内部 identity，避免复制注册表或重新编号。Button/Input 接受同一个服务引用，并注册为非拥有型同步参与者；析构时解除注册。服务负责一次布局后按声明顺序推进各参与者 geometry、effect compaction、fragment 同步、hit-test、focus 和 dirty clear。相比一次性重写 component tree，此方案复用现有流程并保持最小失效边界。

### 2. 组件挂载与清理

窗口服务提供共同 `mount/destroy` 入口；挂载期间通过有作用域的内部 host registration 让现有 `Button`、`Input` 构建函数找到对应 consumer。Button/Input 不互相持有指针。服务先取消指针捕获再销毁组件和相关资源；回调可同步销毁当前组件，所有后续访问必须重新检查 generation。相比保留 Input 对 Button 的入口依赖，此方案让新增控件只需注册自身构建器与同步参与者。

### 3. 单一编辑设施

独立 `WindowTextEditServices` 由窗口服务按需绑定平台端口和剪贴板端口，持有现有 `TextEditorStore`、`TextInputSessionHost`、`TextClipboardCommands`；Input 只借用它们。一个窗口只接受一次平台绑定，第二次绑定同一端口复用，冲突端口报错，不为每个文本控件类型建立另一会话。编辑、IME、selection、caret 映射和 display projection 仍由 Input 专属逻辑处理，后续 Search/Password 按实际策略扩展。相比现在由 Input 类型拥有会话，这一所有权边界不会强迫未来文本控件依赖 Input。

### 4. 可组合按压行为

内部 `PressableBehavior` 仅维护 primary pointer identity、pressed、capture 和释放判定，输出 `pressed_changed` 与一次性的 activation intent。hover、禁用/loading gate、keyboard Enter/Space、回调与视觉由 Button 决定。行为在 release/cancel/disable/destroy/window blur 时先清理捕获及 pressed，再允许 activation 回调；旧 generation 事件不得激活新组件。相比复制 `handle_pointer`，新控件可共享手势规则而独立定义 toggle 语义。

### 5. 验证与阶段边界

先完成窗口服务和现有 Button/Input 适配，再迁编辑对象所有权，最后提取 Pressable。每阶段用既有 CTest、混合挂载/销毁及性能证据核对；公开 header 不含 SDL3 或内部服务类型。正式构建使用 `CMakePresets.json` 的 `Ninja Multi-Config`，本机 Windows 采用 MSVC x64；平台通用合同只在一个受支持平台执行一次。只有窗口系统、真实 IME、GPU/driver/font/DPI 相关行为需要 Windows 与 Linux 独立验收，013 的纯内部重构不声称补齐此前未完成的人工视觉或 Wayland 证据。

## Risks / Trade-offs

- [同步顺序变化导致 hit-test、scene 或 dirty range 回归] → 保留原顺序，加入混合组件和 dirty/upload 断言，失败时仅回退对应阶段。
- [构造/析构顺序导致悬空参与者] → 服务晚于 consumer 析构，注册使用显式 attach/detach，并测试窗口销毁和回调自毁。
- [文本平台端口被重复绑定] → 单窗口显式绑定和冲突检查，保持当前平台会话的唯一性。
- [按压提取把 Button 的键盘语义带给其他控件] → 行为层不处理键盘，也不调用组件回调。

## Migration Plan

先让现有窗口和测试 fixture 显式创建服务，再将 Button/Input 成员和接线迁过去；每个可独立验收的阶段单独提交。若回归失败，回退最近阶段即可恢复原有内部宿主；公开 API 和数据格式不迁移。
