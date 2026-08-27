## Purpose

定义 RynUI 首个稳定交互组件的公开 typed Props、typed content、Ant Design 6 视觉状态与 click 行为，使应用可以用 reactive Button 完成 pointer/keyboard 操作而不接触 Runtime、Scene 或 SDL3。

## ADDED Requirements

### Requirement: 公开 Button API 保持 typed 与平台无关
系统 SHALL 在 `ryn` 命名空间公开 `ControlSize::{Small, Middle, Large}`、`ButtonType::{Default, Primary}`、`ButtonProps`、Button 专用 typed content slot 与 `ryn::Button`。`type`、`size`、`disabled`、`loading` 和 `layout` SHALL 使用对应 typed value 或 `Prop<T>`，`onClick` SHALL 接受 owning callback；公开 API MUST 不包含任意 color、font、border、radius、shader、PrimitiveStyle、内部 identity 或 SDL3 类型。

#### Scenario: 公开 DSL 编译
- **WHEN** consumer 只包含 `ryn/rynui.hpp`，以 static value、`Signal<T>` 或 `Binding<T>` 构造 Button Props，并传入 `[] { ryn::Text(u8"确定"); }` content
- **THEN** 代码以 C++20 编译，Button 的类型、尺寸、disabled/loading 和外部 `LayoutStyle` 归一化到同一 typed Props 路径

#### Scenario: 非法视觉入口不可用
- **WHEN** consumer 尝试通过 Button Props 设置任意颜色、字体、边框、圆角、shader 或通用视觉 Modifier
- **THEN** 对应 API 在类型层不存在，Button 视觉只由 Theme 与 Button Component Token 决定

#### Scenario: content slot 类型隔离
- **WHEN** consumer 使用 Button 支持的 content slot 或尝试传入未声明的 prefix、suffix、footer slot
- **THEN** content slot 可编译并按声明挂载，未支持 slot 在类型层不可用

### Requirement: Button 测量与 content composition
Button SHALL 拥有一个可交互 root 和持久化 content subtree。自然尺寸 MUST 由 content intrinsic size、当前 ControlSize 的内部 padding/border/control height 与外部 constraints 共同确定；Small、Middle、Large 的默认 control height SHALL 分别为 24、32、40 logical pixels。外部 `LayoutStyle` 只影响 Button 与父布局的关系，不覆盖内部 padding 或视觉 token。

#### Scenario: Text content 决定自然宽度
- **WHEN** Default/Middle Button 的 Text content 从短文本变为更长的 Latin/CJK 文本且未设置固定宽度
- **THEN** 只重新测量该 Button content 与必要祖先，Button 高度保持 32 logical pixels，宽度按 shaped metrics 与 token padding 更新

#### Scenario: 外部宽度约束
- **WHEN** Button 设置有效 fixed/min/max width 或 margin
- **THEN** root 在不创建外部 wrapper Node 的情况下遵守约束，content 在扣除内部 padding 后居中，margin 不改变 Button Component Token

#### Scenario: content closure 不因属性变化重跑
- **WHEN** type、size、disabled、loading、hover、pressed、focused 或 `LayoutStyle` 响应更新
- **THEN** Button 与 Text 保持原 component identity，content closure 不重新执行，除非发生显式结构变更

### Requirement: Button 使用 Ant Design 6.5.0 状态视觉
Button SHALL 从不可变 Default Theme snapshot 读取锁定的 Button token。Default 与 Primary 在 default、hover、pressed、focus-visible、disabled 和 loading 状态 MUST 具有可区分且符合 Ant Design 6.5.0 层级的背景、边框、前景、圆角、尺寸与状态反馈；disabled SHALL 优先于 hover/pressed，loading SHALL 使用 loading 视觉并抑制重复操作。首批实现 MUST 不引入 React、DOM 或 CSS-in-JS 依赖。

#### Scenario: Primary 状态序列
- **WHEN** enabled Primary/Middle Button 依次进入 default、hover、pressed、focus-visible 和 loading
- **THEN** 背景/前景/focus visual 按锁定 token 变化，content 保持可读，且每次只更新目标 Button 的必要 scene ranges

#### Scenario: Disabled 优先级
- **WHEN** disabled Button 同时收到 pointer move/down 或 keyboard activation
- **THEN** 视觉保持 disabled，不进入 hover/pressed，不执行 `onClick`，也不进入 focus order

#### Scenario: Button content 继承受控前景色
- **WHEN** Button content 中的 Text 未显式指定 tone
- **THEN** Text 继承 Button 当前前景 token，使 Primary 使用浅色前景、Default 使用正文前景；该继承不向公开 API 暴露任意颜色 override

### Requirement: Pointer click 只在完整激活手势后触发
enabled 且非 loading Button SHALL 在主 pointer down 命中时获得 pressed 与 capture，并在同一 pointer 于 Button 有效 bounds 内抬起时同步执行一次 `onClick`。非主按键、bounds 外抬起、cancel、窗口失焦、disable/loading 变化或销毁 MUST 不触发 click。

#### Scenario: 正常 pointer click
- **WHEN** 主 pointer 在 Button bounds 内 down 并在同一 Button bounds 内 up
- **THEN** Button 依次显示 pressed、释放 capture、清除 pressed，并在 UI owner thread 同步执行一次 `onClick`

#### Scenario: loading 防止重复提交
- **WHEN** 第一次 `onClick` 将 reactive loading 设为 true，随后在其恢复前再次 pointer 或 keyboard 激活
- **THEN** 后续激活不执行 `onClick`，已有 focus identity 可以保留

#### Scenario: 回调销毁自身
- **WHEN** `onClick` 回调销毁当前 Button 或其父 Scope
- **THEN** capture 与 pressed 已在回调前收口，回调返回后不访问 Button state、content subtree 或已复用 scene range

### Requirement: Button 键盘与焦点行为一致
Button SHALL 使用统一 Focus manager 提供 `Tab`/`Shift+Tab`、focus-visible、`Enter` 和 `Space` 行为。Pointer 与 keyboard 激活 MUST 汇入同一个 click callback 路径，不得因同一次操作重复触发。

#### Scenario: Keyboard 与 pointer 共用回调
- **WHEN** 同一 Button 分别由 pointer、`Enter` 和 `Space` 完成三次有效激活
- **THEN** 同一个 `onClick` callback 总计执行三次，每次执行前状态均已完成相应 pressed/capture 收口

### Requirement: Reactive Button 更新保持最小失效
Button Props 更新 SHALL 原子校验后应用：type、disabled、hover、pressed 或 focus 状态变化只更新目标 Material/interaction/focus visual；size、loading indicator 或影响 content area 的 token 变化触发目标 Measure/Layout；外部 placement 只更新 geometry/HitTest；loading/disabled 改变 SHALL 同步更新交互资格。普通更新 MUST 不重新 shape 未变化文本、不重建无关 Button、不全量上传 scene，也不得在 idle 时持续 submit。

#### Scenario: Hover 只更新目标 visual
- **WHEN** pointer 在两个 Button 之间移动而其 bounds、Text 和 Props 不变
- **THEN** 只更新离开与进入 Button 的 material ranges，不发生 Text shaping、content mount 或全树 Layout

#### Scenario: Size 更新局部布局
- **WHEN** 一个 Button 的 reactive size 从 Middle 改为 Large
- **THEN** 只重新测量该 Button 与必要祖先并更新其 geometry/HitTest/visual，兄弟 Button identity 与 scene range 保持稳定

#### Scenario: 稳定后停止提交
- **WHEN** 所有 pointer、focus、Prop 和布局更新已经完成且没有动画任务
- **THEN** frame loop 返回 idle，不按显示刷新率持续提交 GPU frame

### Requirement: 生命周期与公开依赖保持封闭
Button Scope 销毁 SHALL 先停止 Prop 与事件订阅，再释放 focus/capture/HitTest 注册和 Button/Text scene ranges，最后销毁 Node subtree 并推进 generation。公开 Button consumer SHALL 只依赖 C++20 与 RynUI public target。

#### Scenario: 销毁后 Signal 写入
- **WHEN** Button 销毁后继续写入其 type、size、disabled 或 loading Signal
- **THEN** 不产生 callback、frame request、HitTest/focus 变化或 scene upload

#### Scenario: 公开依赖隔离
- **WHEN** 对 Button headers 和 public target 运行 include/link dependency contract
- **THEN** 不出现 Runtime、Node、Scene、Font、HarfBuzz、FreeType、SDL3 或 GPU backend 类型与链接依赖
