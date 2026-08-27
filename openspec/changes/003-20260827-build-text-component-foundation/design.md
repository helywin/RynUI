## Context

见 `proposal.md` 的动机与范围。当前公开 facade 只导出 Reactive、String 和 Version；组件挂载、Node/Layout、Font/Text state、Glyph scene、Platform 与 Renderer 都位于内部模块。001 的 `ComponentInstance` 只能通过显式 `MountContext` 手工创建 Node，002 的 `TextRenderController` 只管理一个由示例直接组装的 Text，因此它们尚不能承载公开声明式组件。

本设计必须同时保持四条已有边界：组件函数只在首次挂载时执行；Text measurement 来自 shaped metrics；Font/Text/Glyph/GPU 状态属于 UI owner thread；稳定组件视觉只读 Theme/Token，`LayoutStyle` 不成为视觉 override。完整 public `Application`/`Window`、Theme 切换和结构 reconciliation 不在本 change 中固化。

## Goals / Non-Goals

### Goals

- 建立可从 `rynui.hpp` 使用的 `Prop<T>`、typed content DSL、`LayoutStyle`、`TextProps` 与 `ryn::Text`。
- 让公开 DSL 通过内部 application host 挂载到稳定、generation-checked 的 component/Text identity。
- 使多个 Text 共享 Font Runtime、GlyphAtlas 和 Renderer，同时保留逐 Text 的 shape/measure/layout/material 失效范围。
- 用只读 Default Theme snapshot 提供 14 logical-pixel 正文及 primary/secondary/disabled 语义，不开放第二套视觉入口。
- 使后续 HitTest/Focus/Button change 可以复用 component identity、Node bounds、Scope、Text content 和 `LayoutStyle`，而无需改写本 change 的公开值语义。

### Non-Goals

- 不发布 `Application`、`Window`、事件循环或 renderer backend 的稳定构造 API；示例继续由内部 platform/application glue 启动公开组件 content。
- 不实现结构响应、keyed reconciliation、动态 slot 增删、Portal、Context override 或多窗口组件迁移。
- 不实现 Title/Paragraph/strong、字体 weight/variable axis、新字体资源、任意字号/颜色、完整 Theme algorithm 或运行时主题切换。
- 不实现 Pointer、Keyboard、Focus、Button、Input、IME、Selection、Accessibility、公开 Flex/Space 或高阶布局。

## Decisions

### 1. `Prop<T>` 保存静态值或 type-erased reactive reader

公开 `Prop<T>` 使用两种存储形态：静态 `T`，或拥有的 `Binding<T>` reader。接受任意 `Signal<T, Equal>` 时按值捕获 Signal 到 Binding；接受 Binding 时同样按值保存其 callable。这样 custom equality 的 Signal 不需要进入 `Prop<T>` 的 variant 类型，临时 Props/Binding 也不会留下裸引用。

组件内部使用 `connect_prop(scope, prop, apply)`：静态值直接应用一次，不创建 Observer；reactive reader 通过现有 binding phase Observer 应用，并在适配器保留最后已应用值以抑制相等更新。`Prop<T>` 自身不包含 Node、Dirty 或组件知识，每个 typed field 的 adapter 决定最小失效。

备选方案是令 `Prop<T>` 保存 `std::variant<T, Signal<T>, Binding<T>>`。这无法覆盖自定义 `Equal` 的 Signal，并会把每种 reactive source 类型传播到组件头，因此不采用。另一个备选是保存引用，临时 Props 和 Binding 会产生生命周期风险，也不采用。

### 2. 公开 DSL 使用 owner-thread mount stack，Host 保持内部

公开 component 函数保持架构示例的无显式 context 写法。内部 application host 在首次挂载 content closure 时压入一个 owner-thread `ComponentBuildContext`，`ryn::Text(...)` 读取当前 context、创建 component record 并立即完成声明；返回后弹出 context。嵌套 typed slot 使用同一 stack 建立父子顺序。非 owner thread 或没有 active host 时调用 component DSL 必须 fail-fast。

active build context 可以使用 `thread_local` 指针栈，因为它只表示当前 UI owner thread 的同步挂载阶段，不承担跨线程通信；这与现有 thread-local Reactive Scheduler 一致，但不得允许 worker 创建 UI component。普通 Prop 更新只访问持久化 record，不再次进入 build stack。

Host 内部拥有 component slot 表，identity 为 slot + generation；record 持有 Scope、root Node、类型 tag 和类型专用 state。销毁顺序为停止 Scope/Observer、移除 Scene range、释放类型 state、销毁 Node 子树、推进 generation。公开 API 不暴露 Host、record 或 identity，后续 public Application change 只需创建同一 Host 并提交 content closure。

备选方案是把 `MountContext&` 作为每个公开组件的第一个参数。它更显式，但与已批准的 C++ DSL 形态冲突，并会把内部树构建对象扩散到所有 slot，因此不采用。此 change 也不发布临时 `Application` facade，以免把字体路径、GPU backend 与单窗口限制固化为长期 API。

### 3. `LayoutStyle` 编译到组件同一个 root Node，不生成 wrapper

公开 layout 值使用 strong logical-unit 类型表达非负有限尺寸；首批 `LayoutStyle` 字段为 width、height、min/max width、min/max height 与四边 margin，字段可由 `Prop<T>` 驱动。`auto` 与显式 logical length 使用 tagged value 区分，不用负数 sentinel。当前内部 float layout 继续表示 logical pixels；高 DPI 到 physical pixels 的完整 scale contract 留给 application/window change，但公开单位不直接承诺 framebuffer pixel。

Host 把 `LayoutStyle` 编译为 component root Node 的外部 constraint/margin metadata。父 layout 在计算 child constraints 和 placement 时读取这些值；组件内部 Text measurement 只收到扣除 margin 后的 content constraints。不得为了 margin/width 创建额外 wrapper Node，也不得让 `LayoutStyle` 写入 color、font、line height 或 glyph state。

width 或 min/max 改变且有效 content constraint 变化时标记 `Measure|Layout`；只改变 margin 且 content constraint 不变时标记 placement/geometry。非法值在写入 metadata 前验证，失败不得留下部分更新。

备选方案是直接公开内部 `Constraints`、`Padding` 或 `FlexLayout`。这些类型是 engine policy，不是 Ant Design 公开布局合同，而且 padding 会与组件内部 Token 竞争，因此不采用。

### 4. Text leaf 通过 intrinsic measure adapter 接入 Layout

现有 `LeafLayout` 只返回固定 preferred size，不能表达内容和约束相关的 Text。Layout 模块增加平台无关的 intrinsic measure adapter 注册表，以 Node generation 为 key；Text component 注册一个 adapter，根据当前 shaped result、Theme line height 和传入 constraint 调用既有 Text measurement。Layout 不包含 Font、HarfBuzz、Glyph 或 Renderer header，只通过 RynUI 自有 size/result 协议读取测量结果。

content/font/size revision 先使 Text shape dirty，再使 intrinsic measure 与祖先 layout dirty；width constraint 只重新 measure/wrap，不重新 shape；最终 place 只更新 glyph translation/clip。adapter 在 component dispose 时先注销，stale Node generation 不得调用旧 Text state。

备选方案是由 Text component 在 layout 之前自行计算固定 size。这会复制 constraint 传播并使父布局看不到正确 intrinsic size；让 Layout 直接依赖 TextEngine 又会破坏模块方向，因此均不采用。

### 5. 单 Text controller 泛化为 Host 级 Text scene service

002 的 `TextRenderController` 持有单个 TextState、GlyphAtlas 和 GlyphScene。本 change 将共享资源提升为 Host 级 text scene service：

```text
Internal Application Host
  |- Component slots / Scope / NodeStore / Layout
  |- FontRuntime + immutable DefaultThemeSnapshot
  `- TextSceneService
       |- Text record A -> TextState + stable Glyph range
       |- Text record B -> TextState + stable Glyph range
       |- shared GlyphAtlas / GlyphScene / ordered commands
       `- renderer-facing dirty/upload plan
```

每个 Text record 独立保存 content/tone/layout revision、TextState、measurement cache、Node identity 与 Scene range generation。共享 atlas 保持既有 append-only entry；Text range 的删除和复用必须推进 generation，并在构建 ordered scene 时保持组件声明顺序。销毁一个 Text 只删除其 range，不释放仍被其他 Text 使用的 atlas entry。

该 service 仍是 engine/internal API，不进入 `include/ryn/`。Renderer 只消费聚合后的 ordered scene、dirty instance ranges 和 atlas upload plan，不读取 Props 或 component record。

备选方案是每个 Text 拥有独立 atlas/controller，容易实现但会重复 rasterization、texture 和 draw binding，也无法证明真实组件共享资源，因此不采用。

### 6. `TextProps` 只公开内容、tone 与外部布局

首批 API 形态收敛为一个 typed Props builder：

```cpp
void StatusContent(const ryn::Signal<ryn::String>& status)
{
    ryn::Text(
        ryn::TextProps{}
            .content(status)
            .tone(ryn::TextTone::Secondary)
            .layout(
                ryn::LayoutStyle{}
                    .max_width(ryn::dp(320))
                    .margin_bottom(ryn::dp(8))));
}
```

`content` 归一化为 `Prop<String>`，不保存 `StringView`；`tone` 归一化为 `Prop<TextTone>`；`layout` 使用 `LayoutStyle`。`u8` literal 继续由 String array constructor 进入 owning value。convenience overload 可以接受 `String`/`u8` literal，但最终必须调用同一 `TextProps` 路径。

不公开 font family、font size、line height、color、opacity、shader 或 PrimitiveStyle。Text component 从 Host 的 immutable `DefaultThemeSnapshot` 读取 typography body token 与 primary/secondary/disabled alias color。snapshot 的字段和值以 Ant Design 6.5.0 参考和 002 已验证的 14px 正文边界锁定在 contract test；公开 Theme 配置和响应式 token context 后置，不为本 change 增加假的 override API。

tone 更新只写 Material range；content 更新重新 shape/measure 目标 Text；LayoutStyle constraint 更新只重新 measure/layout；placement 更新只写 geometry。每条 adapter 都输出诊断计数和 Dirty reason，供测试证明边界。

### 7. 公共 header 与 target 保持严格分层

新增公开 header 只包含 C++20 标准库和其他 `include/ryn/` 类型。`Prop<T>` 放在 Reactive 之上的公开值层；component/Text headers 不包含 `src/`。`rynui.hpp` 导出稳定入口；public-header isolation 和 forbidden include scan 扩展到所有新增 header。

内部 target 依赖保持单向：component runtime 可以依赖 Reactive、Layout、Text scene service；Layout 只依赖 Runtime 自有协议；Text scene service 依赖 Font/Text/Graphics；Renderer 不反向依赖 components。SDL3 仍只在 platform/renderer target 中出现。

### 8. 验收按声明、生命周期、失效与真实窗口分层

自动测试至少覆盖：Prop 三种来源和临时生命周期；mount stack/owner thread/slot 顺序；Scope dispose 与 generation；LayoutStyle validation/constraint/margin；Text intrinsic measurement；多个 Text 共享 glyph；content/tone/layout/placement 的精确计数；public header isolation。

真实窗口示例使用公开 `ryn::Text` content 声明 primary、secondary、disabled 的 Latin/CJK 文本，由内部 host/platform glue 启动。Windows 使用 `windows-msvc` Debug/Release、D3D12/DXIL；Linux 使用 `linux-gcc` Debug/Release 与 `linux-clang` Debug，真实窗口为 GCC/Vulkan/SPIR-V。保存截图、driver、exit code、mount/Prop/shape/measure/layout/atlas/instance/draw/submit/idle 计数。Clang build 继续使用关闭 extensions 的标准 C++20 模式。

`tasks.md` 将 Windows 与 Linux 的 build、CTest、真实窗口和 evidence contract 拆为独立章节与 checkbox，使每台开发电脑只推进当前平台清单。平台无关的实现和 headless contract 单独验收；最终跨平台收口只汇总两个平台已经完成的结果，不替代任一平台证据，也不因另一平台尚未执行而回退已完成项。

源代码 build、CTest 和 headless contract 不能替代 Windows/Linux 真实窗口视觉验收；同样，公开 DSL 编译通过不代表 public Application API 已交付。

## Risks / Trade-offs

- **[隐式 mount context 难以误用诊断]** → 在无 active Host、错误线程或重入阶段调用时返回明确 fail-fast；测试嵌套 slot 和异常恢复后 stack 完整性。
- **[`Prop<T>` type erasure 可能产生分配]** → 静态 Prop 不分配；Binding 构造期分配允许，steady-state update 继续计数并禁止每次更新分配，必要时后续加入 small-function optimization 而不改 API。
- **[Text intrinsic measurement 可能形成 Layout/Text 循环]** → adapter 只接受单向 constraints 并返回 size/line result；同一 layout generation 内禁止递归请求父 layout，使用 revision cache 防止重复 shaping。
- **[多个 Text 删除造成 instance range 移动]** → range 使用 generation 与显式 remap；允许删除时局部 compact instance store，但不得改变 surviving component identity 或 atlas UV。
- **[只读 Default Theme 可能被误认为完整主题能力]** → public API 不暴露 Theme override，文档和诊断只声明固定 snapshot；Default/Dark/Compact 与 Component Token override 保持后续任务。
- **[逻辑单位尚未完成高 DPI 应用合同]** → public length 使用 strong logical unit，不暴露 framebuffer pixel；本 change 的真实窗口记录 display scale，并仅声明已实际验证的 scale，不外推到未测 DPI。
- **[现有内部示例与新 component host 双路径漂移]** → 保留低层 engine tests，但真实 Text 示例迁移到公开 DSL；禁止复制 shaping/atlas 逻辑到 component 层。

## Migration Plan

本 change 为 additive，现有 Reactive/String 和低层 Text engine API 保持兼容。实施先落 `Prop<T>` 与 public header contract，再建立 internal host/component identity 和 `LayoutStyle`，随后泛化多 Text scene service、接入 intrinsic measurement，最后发布 `TextProps`/`ryn::Text` 并迁移真实窗口示例。

每个阶段保持已有 001/002 测试通过并独立提交。若 component host 或 Text adapter 失败，可回退对应阶段而继续保留 002 的单 Text engine demo；不得删除低层测试来使新路径通过。完成后同步 delta specs；只有全部任务和双平台证据完成且用户明确要求时才 archive。
