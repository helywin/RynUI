# RynUI 最终架构与实现路线

## 1. 文档状态

本文是 RynUI 当前唯一的正式架构基线。早期讨论中的 `PulseUI` 统一更名为 `RynUI`，公开 C++ API 统一使用 `ryn` 命名空间。

本文描述目标、设计约束和实施路线，不代表对应功能已经实现。实现行为以 OpenSpec change、源代码、测试和真实窗口验收结果为准。

## 2. 产品定位

RynUI 是一个桌面优先、GPU-first、细粒度响应式的现代 C++ UI 框架：

> Ant Design-native Typed Component API + C++ Declarative Slot DSL + SolidJS 式 Fine-grained Reactivity + Retained UI Tree + Constraints/Phased Invalidation + 专用轻量 GPU Renderer。

主要目标场景包括桌面工具、工业控制、机器人应用、监控面板、IDE 类工具以及包含大型 Table/Tree 的数据密集界面。

### 2.1 核心目标

- 状态更新只影响真正依赖该状态的属性或节点。
- 颜色、透明度和变换等高频更新尽量只修改 GPU instance 数据。
- 闲置窗口不持续满帧渲染。
- 从第一阶段支持 UTF-8、中文字体回退与桌面输入模型。
- Windows、Linux 和 macOS 共享统一的上层运行时与渲染抽象。
- 架构允许按需替换平台和渲染后端，而不污染组件 API。
- 基础 UI 组件、公开布局语义、Design Token、主题算法和交互状态统一参照 Ant Design 6。

### 2.2 非目标

- 第一阶段不追求完整商业组件库。
- 第一阶段不实现通用 2D 图形引擎或完整 SVG/PDF/滤镜能力。
- 不复制 Web DOM/CSS 全部语义。
- 不以每帧重建整棵 UI 描述树作为默认响应模型。
- 不承诺在没有 benchmark 和真实窗口证据时达到具体性能数字。
- 不复制 Ant Design 的 React、DOM 或 CSS-in-JS 实现；RynUI 只把其设计语言和行为合同映射到原生 C++ Runtime。

## 3. 总体架构

```text
Application
    |
    v
RynUI C++20 DSL
    |
    v
Reactive Runtime
Signal / Memo / Effect / Binding / Scope
    |
    v
UI Runtime
Component / Typed Props / Slots / Node / If / For / Context
    |
    v
Layout Engine
Constraints / Measure / Place / Scroll / Virtualization
    |
    v
GPU Scene
Quad / Glyph / Image / Clip / Layer
    |
    v
RynUI Lightweight Renderer
QuadBatch / GlyphBatch / ImageBatch
    |
    v
SDL3 GPU
D3D12 / Vulkan / Metal
```

SDL3 同时作为第一阶段的平台层，负责 Window、Mouse、Keyboard、IME、Clipboard、Cursor、Touch 和 GPU device 接入。上层模块不得直接依赖具体 D3D12、Vulkan 或 Metal 类型。

## 4. 不可破坏的设计原则

### 4.1 不使用 Virtual DOM

组件在首次挂载时建立持久化 Node 和响应依赖。普通状态变化不重新执行整个组件函数：

```text
首次挂载：Component -> Node Tree + Reactive Dependencies

后续更新：Signal -> Binding -> Node Property / RenderPrimitive
```

只有 `If`、`For` 或显式 `Reactive` scope 处理局部结构变化。

### 4.2 Component 与 Node 分离

`Component` 是函数及其生命周期 `Scope`，不是渲染节点。真正参与布局、命中测试和绘制的是 `Node`。

```text
Component
  + Scope
      - Signal
      - Memo
      - Effect
      - Binding
      - Cleanup
  + Mounted Nodes
```

`Scope` 销毁时必须解除依赖、停止 Effect/Timer、卸载 Node 并释放关联资源。

### 4.3 属性响应与结构响应分离

- Property Reactive：文本、颜色、尺寸、透明度、变换等属性通过 `Binding` 更新。
- Structural Reactive：条件和列表变化通过 `If`、`For` 或局部 `Reactive` scope 更新子树。
- 普通属性变化不得触发无关组件重新执行。

### 4.4 更新成本逐级受控

优先级从低成本到高成本为：

```text
GPU instance update
  < Paint/Primitive rebuild
  < Layout
  < Structural rebuild
```

能只更新 GPU instance 的变化不得触发 Layout；能局部 Layout 的变化不得重建组件子树。

### 4.5 公开组件只有一套样式来源

Ant Design 组件不能同时接受无约束的 Compose-style `Modifier` 和 Design Token，否则 Button 等组件会出现 Token、variant 与任意 background/border/font override 互相竞争的问题。

公开组件必须遵守以下分工：

- typed Props 表达组件语义、variant、size、status、disabled、loading、事件和 typed slot。
- reactive `Prop<T>` 让 Props 接受静态值、`Signal<T>` 或 `Binding<T>`。
- `LayoutStyle` 只控制组件外部 width、height、margin、flex、alignSelf、order 等布局关系。
- Theme 和 Component Token 是稳定组件的唯一视觉样式来源。
- `PrimitiveStyle` 只开放给 `Surface`、`Canvas` 和自定义组件，不作为 Button/Input/Table 等稳定组件的通用入口。

## 5. 公开 API：Typed Props + Slots + Reactive Prop

所有公开 API 使用 `ryn` 命名空间。公开组件名和 Props 语义按 Ant Design 组织；C++ slot/lambda 只负责声明父子关系，不模拟 Kotlin 语法：

```cpp
void CounterPage()
{
    auto count = ryn::state(0);

    ryn::Flex(
        ryn::FlexProps{}
            .vertical(true)
            .gap(ryn::SpaceSize::Middle)
            .layout(
                ryn::LayoutStyle{}
                    .fill()
                    .padding(24_dp)
            ),
        [=] {
            ryn::Title(
                ryn::TitleProps{}.level(4),
                ryn::bind([=] {
                    return std::format("Count: {}", count());
                })
            );

            ryn::Space(
                ryn::SpaceProps{}.size(ryn::SpaceSize::Small),
                [=] {
                    ryn::Button(
                        ryn::ButtonProps{}
                            .onClick([=] {
                                count.update([](int value) {
                                    return value - 1;
                                });
                            }),
                        [] { ryn::Text("Decrease"); }
                    );

                    ryn::Button(
                        ryn::ButtonProps{}
                            .type(ryn::ButtonType::Primary)
                            .size(ryn::ControlSize::Middle)
                            .onClick([=] {
                                count.update([](int value) {
                                    return value + 1;
                                });
                            }),
                        [] { ryn::Text("Increase"); }
                    );
                }
            );

            ryn::If(
                [=] { return count() >= 10; },
                [] { ryn::Text("Count is large"); }
            );
        }
    );
}
```

`ButtonProps::loading()`、`InputProps::status()`、`FlexProps::gap()` 等响应式字段在类型层使用 `Prop<T>`，可接受静态 `T`、`Signal<T>` 或 `Binding<T>`。typed slot 用 lambda 承载 `content`、`icon`、`prefix`、`suffix`、`title`、`footer` 等结构；组件只暴露其真实支持的 slot。

API 不依赖宏或 Kotlin compiler plugin。Compose 的 slot composition、Constraints 和 phased state read 只作为 Runtime 机制参考；RynUI 不公开通用 Compose `Modifier` 链，也不复制 Ant Design React Props 的 DOM/CSS 字段。

## 6. Reactive Runtime

### 6.1 核心类型

- `Signal<T>`：可读写状态，读取时自动收集当前 Observer。
- `Memo<T>`：缓存派生值，只在依赖失效后重新计算。
- `Effect`：在提交阶段运行副作用，不参与布局或绘制结果计算。
- `Binding<T>`：把响应式表达式连接到具体属性更新器。
- `Scope`：拥有响应对象、清理回调和挂载节点。
- `Scheduler`：合并更新、排序传播并驱动单帧流水线。

### 6.2 依赖图约束

- 依赖在读取 `Signal` 时建立，而不是由调用方手工列出。
- 重算前清理旧依赖，允许动态依赖集合。
- 同一批次的多次写入只排队一次 Observer。
- 循环依赖、Effect 重入和 Scope 销毁后的更新必须有确定行为。
- UI Signal 默认仅允许 UI 线程读写，不为每个 Signal 引入锁。

## 7. Node、Binding 与 Dirty 模型

建议的 Dirty 分类：

```cpp
enum class DirtyFlags : std::uint32_t {
    None      = 0,
    Structure = 1u << 0,
    Measure   = 1u << 1,
    Layout    = 1u << 2,
    Geometry  = 1u << 3,
    Material  = 1u << 4,
    Text      = 1u << 5,
    Image     = 1u << 6,
    Transform = 1u << 7,
    Clip      = 1u << 8,
    HitTest   = 1u << 9,
};
```

| 属性变化 | 最小 Dirty 范围 |
|---|---|
| `background`、`color` | `Material` |
| `opacity` | `Material` 或合成数据 |
| `translation` | `Transform` + `HitTest` |
| `width`、`height`、`padding` | `Measure` + `Layout` + `Geometry` |
| `text` | `Text`，必要时追加 `Measure` |
| `fontSize` | `Text` + `Measure` |
| `clip` | `Clip` + `HitTest` |
| 增删 child | `Structure` + `Measure` + `Layout` |

每种公开属性都必须声明它的最小失效范围，并通过测试证明不会扩大为无关阶段。

## 8. Layout Engine 与公开 LayoutStyle

Layout 使用父节点向子节点传递 `Constraints`、子节点返回尺寸、父节点执行 `place` 的模型：

```cpp
struct Constraints {
    float minWidth;
    float maxWidth;
    float minHeight;
    float maxHeight;
};
```

Layout Engine 内部先实现 `BoxLayout` 和可切换 horizontal/vertical 的 `FlexLayout`；公开布局 API 采用 Ant Design 的 `Flex`、`Space`、`Grid`（`Row`/`Col`）和 `Layout` 语义。响应式断点基于窗口内容区尺寸计算，不使用显示器型号或固定设备硬编码。

`LayoutStyle` 在挂载时编译为 Node 的外部布局属性，不创建额外 Wrapper Node，也不允许覆盖组件内部颜色、边框、字体、状态层或动画。组件内部视觉由 Theme/Component Token 决定；低层 `Surface`/`Canvas` 才接受 `PrimitiveStyle`。

## 9. Ant Design 组件、布局与样式基线

RynUI 的原生 Design System 以 Ant Design 6 为初始基线，本文整理时对应参考版本为 `6.5.0`。后续升级参考版本必须通过独立 OpenSpec change 评估视觉、交互和兼容性影响，不能静默跟随上游变化。

### 9.1 公开组件模型

RynUI 使用自己的 typed C++ API 表达 Ant Design，不采用 Compose-style 通用 `Modifier`，也不照抄 React Props：

```text
Component(
  TypedProps<Prop<T>, Event>,
  TypedSlots<lambda>
)
  + LayoutStyle        # 只控制外部布局
  + Theme/Token        # 控制稳定视觉
  + Binding            # 细粒度更新具体属性
```

Props builder 是规范入口，简单组件可以提供不损失语义的 convenience overload。所有 overload 最终必须归一化为同一个 typed Props/Slots 模型，避免出现两套生命周期或样式行为。

### 9.2 对齐范围

- 组件信息架构与语义：General、Layout、Navigation、Data Entry、Data Display、Feedback 等类别保持一致的职责边界。
- 基础组件：Button、Icon、Typography、Input、Checkbox、Radio、Switch、Slider、Select、Tabs、Menu、Tooltip、Popover、Card、Table、Tree、Modal/Dialog、Drawer、Alert、Message、Notification、Progress、Skeleton 和 Spin 等按 Ant Design 的外观层级、尺寸、状态和交互规则设计。
- 布局：`Flex` 负责主轴/交叉轴、wrap 和 gap；`Space` 负责相邻内容的一致间距；`Grid` 使用 `Row`/`Col`、gutter、span、offset、order 和响应式断点；`Layout` 负责应用级 Header/Sider/Content/Footer 结构。
- 交互状态：default、hover、active/pressed、focus-visible、selected/checked、disabled、loading、success、warning 和 error 必须完整、可预测且跨组件一致。
- 图标、文案、空状态、反馈时机和危险操作层级遵循 Ant Design 的企业级信息表达原则。

对齐目标是视觉与行为合同，不是 React API 的逐字移植。RynUI 可以使用符合 C++ 生命周期和类型系统的 API，但同一组件在相同状态下必须给用户相同的层级、反馈和操作预期。

### 9.3 Layout 响应规则

`Grid` 的断点命名与初始阈值采用 Ant Design 6：`xs=480`、`sm=576`、`md=768`、`lg=992`、`xl=1200`、`xxl=1600`、`xxxl=1920`。阈值以窗口可用内容宽度为输入，并通过 Theme/Scale 配置保留受控覆盖能力。

桌面窗口缩放时，布局必须按断点、flex、wrap、gutter 和 content constraints 重排；不得把单一设备分辨率写死到组件实现。

### 9.4 Design Token 与主题

Theme Runtime 采用与 Ant Design 对应的分层 Token 模型：

```text
Seed Token
  -> Theme Algorithm
  -> Map Token
  -> Alias Token
  -> Component Token
```

- Seed Token 表达品牌输入，例如 `colorPrimary`、字体、基础圆角和尺寸密度。
- Map Token 由主题算法派生完整色板、字体阶梯、间距、阴影和控制尺寸。
- Alias Token 表达跨组件语义，例如 text、surface、border、focus、success、warning、error 和 disabled。
- Component Token 只覆盖具体组件，不允许复制一套脱离全局语义的颜色与间距常量。
- 初始主题算法提供 Default、Dark 与 Compact；组件可以受控覆盖 Token，但默认继承全局算法。

Token 本身是响应式上下文。主题切换只使读取相关 Token 的属性失效；纯颜色主题变化不得无条件触发全树 Layout。

### 9.5 验收方式

每个基础组件在进入稳定 API 前必须提供：

- 与选定 Ant Design 参考版本的状态矩阵和视觉对照。
- Keyboard、Focus、Pointer、disabled/loading/error 等交互验收。
- Default、Dark、Compact 主题与 Component Token 覆盖测试。
- 不同窗口宽度下的 Flex/Grid/Space 响应式布局截图或可重复视觉测试。
- RynUI 特有性能指标，包括 Component 执行、Layout、Primitive 更新和 GPU upload 范围。

## 10. GPU Scene 与 Renderer

### 10.1 Primitive

普通 UI 首先映射为少量 Primitive：

- `QuadPrimitive`：矩形、圆角、边框、渐变、简单阴影。
- `GlyphPrimitive` / `GlyphRun`：文本。
- `ImagePrimitive`：图片和纹理图标。
- `ClipPrimitive`：矩形或圆角裁剪。
- `LayerPrimitive`：需要独立合成的透明度和变换层。

`QuadPrimitive` 应支持 GPU instancing，并允许 `Binding` 直接更新颜色、透明度和变换等 instance 字段。

### 10.2 渲染边界

- `Renderer` 只消费与平台无关的 Scene/Primitive 数据。
- SDL3 GPU 是第一阶段后端，不进入 Reactive、Runtime、Layout 或 Component API。
- 圆角矩形、边框和基础抗锯齿优先使用专用 shader 解决。
- Skia 不属于核心依赖；复杂 Canvas、Path、PDF、滤镜等未来通过可选模块接入。
- 批处理必须保持正确的 Z order、Clip、Texture 和 Blend 语义，不能为了减少 draw call 破坏视觉正确性。

## 11. Text 与桌面输入

文本链路采用：

```text
UTF-8
  -> HarfBuzz shaping
  -> glyph id + position
  -> FreeType rasterization
  -> GlyphAtlas
  -> GlyphInstance
  -> GPU
```

第一阶段使用灰度 coverage atlas，优先保证 12–16px 桌面字号和 CJK 文本的清晰度。文本系统从设计阶段保留 Font fallback、Emoji、IME composition、Cursor、Selection、Clipboard 和 Undo/Redo 所需的数据边界。

输入事件通过 HitTest 定位目标 Node，并支持 Capture、Target、Bubble 三阶段传播。Focus、Pointer capture、Keyboard、IME、Cursor、Clipboard 和 DragDrop 由独立管理器维护。

## 12. 线程与帧调度

UI Runtime 使用单 UI 线程。后台任务通过 `ryn::post()` 或线程安全 Channel 把不可变消息送入 UI 队列。

单帧顺序固定为：

```text
Input
  -> queued writes
  -> Memo propagation
  -> Structural updates
  -> Measure
  -> Layout
  -> Primitive updates
  -> GPU upload
  -> Submit/Present
  -> Effect
```

Scheduler 必须支持批处理、Dirty root 合并、确定性队列顺序，以及更新过程中再次失效时的边界控制。

## 13. 建议工程结构

```text
RynUI/
  CMakeLists.txt
  CMakePresets.json
  cmake/
  include/ryn/
  src/
    base/
    platform/
    reactive/
    runtime/
    layout/
    graphics/
    renderer/
    renderer_sdl/
    text/
    input/
    animation/
    foundation/
    components/
    theme/
    devtools/
  examples/
  tests/
  benchmarks/
  docs/
  openspec/
```

正式构建统一由仓库内的 `CMakePresets.json` 管理，生成器使用 `Ninja Multi-Config`。Windows 使用 MSVC x64 工具链，Linux 分别提供 GCC 与 Clang 配置；Debug/Release 由 build/test preset 的 `configuration` 选择。Windows 配置必须在 Visual Studio Developer Environment 中执行，并在 configure 阶段拒绝 MinGW 等非 MSVC 工具链。个人路径和本机覆盖只进入不提交的 `CMakeUserPresets.json`。

第三方依赖统一由 `cmake/dependencies/` 解析，只允许显式 `BUNDLED` 或 `SYSTEM` 模式。BUNDLED 使用固定 source URL 与 SHA256，SYSTEM 由 vcpkg、Conan、发行版 package 或 superbuild 提供规范 CMake target；不使用 Git submodule，也不提供 system-first 自动回退。SDL3 始终归一为 `SDL3::SDL3`，SDL_shadercross 只作为离线 shader 构建的 host tool，不进入应用运行时链接和公开 API。

模块只能沿架构向下依赖；`reactive` 不依赖 UI，`layout` 不依赖具体 GPU 后端，`components` 不直接调用 SDL3。

## 14. 实现阶段

### Phase 0：工程与验证基线

- C++20、CMake、测试框架与 benchmark 入口。
- SDL3 Window、Event loop、GPU device、clear/present。
- Windows 与 Linux 最小构建和真实窗口运行。

### Phase 1：最小技术闭环

- `Signal`、`Memo`、`Effect`、`Binding`、`Scope`、`Scheduler`。
- 持久化 `Node` 树和基本生命周期。
- 内部 `BoxLayout`、`FlexLayout` 与 Constraints。
- `QuadPrimitive`、instance buffer 与基础 shader。
- `Signal -> Binding -> DirtyFlags -> Node -> QuadPrimitive -> SDL_GPU` 闭环。

### Phase 2：可交互 MVP

- Text、CJK 字体回退、GlyphAtlas。
- HitTest、Pointer、Focus、Button。
- Ant Design 风格的 `Flex`、`Space`、typed `ButtonProps`/slots、reactive `Prop<T>`、`LayoutStyle`、`If`、`For`。
- Scroll、Clip 和基础动画。

### Phase 3：桌面能力

- TextInput、IME、Selection、Clipboard、Undo/Redo。
- VirtualList、VirtualTable、VirtualTree。
- Ant Design 6 对齐的 Seed/Map/Alias/Component Token、Default/Dark/Compact 主题和基础组件集。
- Inspector、Dirty reason 和 Frame profiler。

### Phase 4：扩展能力

- 多窗口、Accessibility 平台适配。
- 复杂动画和自定义渲染扩展。
- 可选 Skia/高级 Path 插件。

## 15. 首个 MVP 验收

首个真实 Demo 使用 Device Monitor 场景，包含动态 CPU、Memory、Temperature、日志列表以及 Start/Stop 操作。

验收必须同时满足：

- 真实窗口能够在目标平台启动、交互和正确退出。
- 后台线程数据通过 UI 队列安全更新。
- 高频数值更新不重新执行页面 Component。
- 纯颜色/透明度/变换更新不触发 Layout。
- 无变化时不持续提交满帧渲染。
- Profiler 能报告 Signal 更新数、受影响 Node、Layout 数、Primitive 更新数、GPU upload 和 draw call。
- 单元测试覆盖依赖收集、批处理、Scope 清理、Dirty 分类和布局约束。

性能目标必须由 benchmark 固化。`0 heap allocations/frame`、10,000 Node、144Hz 动画和 100,000 行虚拟表格属于长期目标，不作为未经验证的当前承诺。

## 16. 关键风险

- Reactive 依赖图、Node 生命周期和结构更新之间可能产生悬空引用或重入问题。
- Clip/Z order/Blend 会限制跨节点批处理，需要优先保证正确性。
- CJK、IME、字体回退和文本选择可能显著扩大 TextInput 复杂度。
- SDL3 GPU 的平台差异需要通过真实 D3D12、Vulkan、Metal 环境验证。
- Ant Design 上游版本演进可能改变 Token、组件状态或响应式布局语义，必须固定参考版本并通过 OpenSpec change 升级。
- 过早扩展组件库会掩盖核心闭环问题，因此阶段边界必须由 OpenSpec change 和验收证据控制。

## 17. 决策摘要

| 主题 | 最终决策 |
|---|---|
| 项目名 | `RynUI` |
| C++ 命名空间 | `ryn` |
| 语言标准 | C++20 |
| 响应模型 | Fine-grained `Signal`，默认不重跑 Component |
| UI 树 | Retained UI Tree |
| 布局 | Constraints + Measure + Place |
| 公开组件 API | Typed Props + typed slots + reactive `Prop<T>` |
| Compose 借鉴范围 | Slot composition、Constraints、phased invalidation；不使用通用 `Modifier` 作为组件 API |
| 公开布局 | Ant Design `Flex` / `Space` / `Grid` / `Layout` 语义 |
| 基础组件与样式 | Ant Design 6.5.0 初始设计基线 |
| 主题 | Seed / Map / Alias / Component Token + Default / Dark / Compact |
| 平台/GPU | SDL3 + SDL3 GPU |
| 普通 UI 渲染 | 自研 Primitive/Batch Renderer |
| 文本 | FreeType + HarfBuzz + GlyphAtlas |
| Skia | 可选插件，不进入核心 |
| 第一优先级 | 证明最小响应式 GPU 闭环 |
