## Context

参见 [proposal.md](proposal.md) 的动机与范围。RynUI 当前是只有正式架构文档的空工程，本 change 需要建立首个可运行基线，并验证 [application-runtime](specs/application-runtime/spec.md)、[reactive-runtime](specs/reactive-runtime/spec.md) 和 [reactive-gpu-update-loop](specs/reactive-gpu-update-loop/spec.md) 的行为合同。

实现必须遵守 [RynUI 最终架构](../../../docs/architecture.md)：公开 API 使用 `ryn` 命名空间，不使用 Virtual DOM，普通状态更新不重新执行整个 Component，SDL3 只存在于平台与渲染后端边界。后续组件层采用 Ant Design-native typed Props、typed slots 和 reactive `Prop<T>`，本 change 不得引入与该模型冲突的通用视觉 `Modifier`。

SDL3 当前约束会直接影响生命周期设计：Window 必须先被 GPU device claim 才能获取 swapchain texture；claim、swapchain acquire、command buffer acquire/submit 都留在创建 Window 的 UI 线程；窗口最小化时得到空 swapchain texture 是正常状态；销毁时先 release Window 与 GPU device 的关系，再销毁 device 和 Window。

## Goals / Non-Goals

**Goals:**

- 建立能在 Windows 与 Linux 重复 configure、build、test 和运行的 C++20/CMake 基线。
- 用安全、可测试的对象生命周期实现细粒度响应语义，而不提前引入每帧分配。
- 用最少的 Layout 与 Quad 渲染能力证明从 `Signal` 到 GPU 可见结果的局部更新。
- 为每个阶段提供计数和测试入口，使“没有无关 Component/Layout/Scene 更新”能够被验证。
- 让平台、响应运行时、UI Runtime、Layout 和 Renderer 保持单向依赖。

**Non-Goals:**

- 本 change 不承诺稳定 ABI；首批 API 只建立可继续演进的源代码级边界。
- 不实现 Text、Button、Scroll、结构响应、动画、完整输入路由或组件主题。
- 不为了长期峰值性能提前实现完整 Arena、SlabAllocator 或无锁队列。
- 不把 SDL3、shader 编译器或测试工具的类型暴露到公开 `ryn` API。

## Decisions

### 1. 工程目标按架构层拆分

CMake 目标按以下依赖方向建立：

```text
rynui_reactive
      |
      v
rynui_runtime -> rynui_layout -> rynui_graphics
                                  |
                                  v
                         rynui_renderer_sdl
                                  |
                                  v
                           rynui_platform_sdl

rynui = public facade target
```

公开头文件位于 `include/ryn/`，公开类型位于 `ryn`；内部实现位于 `ryn::detail` 或不导出的源文件。测试直接链接最小目标，避免所有测试都依赖 SDL3/GPU。

备选方案是先建立单个静态库。该方案文件更少，但会让纯响应测试被平台依赖污染，也无法通过链接边界阻止向上依赖，因此不采用。

### 2. SDL3 使用固定版本的 vendored source，并保留 system package 开关

默认构建从 `third_party/SDL` 的固定 Git revision 使用 `add_subdirectory()`；高级用户可以打开 `RYNUI_USE_SYSTEM_SDL3`，改用 `find_package(SDL3 CONFIG REQUIRED)`。两条路径都只链接 SDL3 官方 CMake target `SDL3::SDL3`。

默认 vendored source 提供一致的 Windows/Linux 基线，system package 开关便于发行版集成。SDL revision 和依赖许可记录必须进入仓库文档，不能跟随浮动分支。

备选方案是构建时直接下载最新 SDL3。该方案无法稳定复现，也会让离线配置失败，因此不采用。

### 3. Shader 使用离线编译，不在应用运行时转换

Quad shader 的单一源文件使用 HLSL。构建阶段调用固定版本的 `SDL_shadercross` CLI，为本 change 生成 Windows/D3D12 所需的 DXIL 和 Linux/Vulkan 所需的 SPIR-V；生成物位于 build tree，不手工编辑。

配置阶段必须检查 shader 工具链并给出可执行的缺失依赖错误。应用运行时只加载与当前 SDL GPU driver 匹配的已编译 shader，不携带运行时转换依赖。macOS/MSL 输出留给后续 change。

备选方案包括运行时调用 `SDL_shadercross` 和分别维护多份 shader 源码。前者增加部署依赖与启动失败面，后者容易发生语义漂移，因此不采用。

### 4. 平台对象使用显式 RAII 状态机

应用平台层按以下顺序取得资源：

```text
SDL init
  -> Window
  -> GPU device
  -> claim Window
  -> render resources
```

释放顺序严格相反。每一步失败都返回包含 SDL error 的结构化结果，并只回滚已经成功的步骤。

每帧由 UI 线程 acquire command buffer，等待并获取 swapchain texture，记录 render pass，结束 pass 后 submit。同一 command buffer 只在 acquire 它的线程提交。窗口最小化导致 swapchain texture 为空时，不进入 render pass，但仍按 SDL3 合同正确结束或提交 command buffer。

备选方案是把 GPU 提交放到独立 render thread。首个闭环不需要跨线程吞吐，且 SDL3 的 Window/swapchain 线程约束会显著增加同步复杂度，因此推迟。

### 5. Reactive Runtime 先保证生命周期正确，再替换存储优化

`Signal<T>` 是共享持有 `SignalCell<T>` 的轻量值类型；创建 Signal 时允许一次堆分配，普通 read/write 不分配。`Observer` 由 `Scope` 稳定拥有，依赖边在每次响应计算前解除并在读取时重建。

`Scheduler` 使用去重队列和单调 epoch：

1. Signal 写入比较新旧值。
2. 有效变化把直接观察者加入当前 dirty queue。
3. `batch()` 只增加 batch depth，最外层结束后开始传播。
4. 先稳定 `Memo`，再更新属性 Binding，最后运行 Effect。
5. Effect 产生的新写入进入下一轮 epoch，不同步递归。
6. 单次 flush 超过保护轮数时返回可诊断错误，防止响应循环卡死。

本 change 允许对象创建和结构挂载时分配，但 steady-state Signal 更新路径不得分配。后续可以把共享所有权和容器替换为 intrusive/pool 实现，而不改变公开语义。

备选方案是第一版直接实现完整 intrusive graph 和自定义 allocator。它的调试成本会妨碍验证行为合同，因此不采用。

### 6. Node 使用 generation handle，Component 只负责首次挂载

Node 存放在拥有稳定 slot 的容器中，以 `{index, generation}` 形式对外引用。删除后增加 generation，旧 handle 必须被诊断为失效，不得访问复用后的 Node。

Component mount 创建 `Scope` 和 Node；普通 Binding 更新只修改 Node property。首个 change 不实现 `If`、`For` 和 subtree reconciliation，因此 Node 结构在挂载完成后保持不变。

备选方案是让公开 API 暴露裸 Node 指针。该方案无法可靠诊断销毁后的访问，也会阻碍未来结构响应，因此不采用。

### 7. Layout Engine 只实现内部 Box 与 Flex 核心

`Constraints` 在入口验证 `min <= max`。Measure 返回 size，Place 写入最终 bounds；每个 Layout pass 使用递增 generation，测试可以读取节点的 measure/place 次数。

首个闭环内部只支持 `BoxLayout`、可切换 horizontal/vertical 的 `FlexLayout` 以及固定尺寸、fill、padding 和 gap 所需数据。公开布局层未来按照 Ant Design 6 提供 `Flex`、`Space`、`Grid`（`Row`/`Col`）和 `Layout`；本 change 不把内部 Layout 类型承诺为稳定公开 API。Grid、Scroll、intrinsic measurement 和 baseline alignment 不进入本 change。

### 8. Ant Design 决定公开组件合同，Compose 只提供机制参考

RynUI 的基础组件、公开布局、Design Token、主题算法和交互状态以 Ant Design 6 为设计基线；初始对照版本为 `6.5.0`。RynUI 只映射设计语义到原生 C++ Runtime，不依赖 React、DOM 或 CSS-in-JS。

Token 层级固定为 Seed -> Map -> Alias -> Component，首批主题算法为 Default、Dark 和 Compact。组件必须覆盖 default、hover、active/pressed、focus-visible、selected/checked、disabled、loading、success、warning 和 error 等适用状态。

Compose 的 slot composition、Constraints 和 phased state read 与 Ant Design 可以共存，因为它们分别解决 Runtime composition 和产品 Design System；但 Compose-style 通用 `Modifier` 与 Ant Design Component Token 都能改变颜色、边框、字体和状态层，会产生两个视觉真源，因此不作为公开组件模型。

后续公开组件统一归一化为：

```text
Component(
  TypedProps<Prop<T>, Event>,
  TypedSlots<lambda>
)
  + LayoutStyle
  + Theme/ComponentToken
```

`Prop<T>` 接受静态值、`Signal<T>` 或 `Binding<T>`；typed slots 只开放组件真实支持的 `content`、`icon`、`prefix`、`suffix`、`title`、`footer` 等结构。`LayoutStyle` 只控制外部布局，Theme/Component Token 是稳定组件的唯一视觉样式入口，`PrimitiveStyle` 仅供 `Surface`、`Canvas` 和自定义组件使用。

本 change 不实现上述组件层，但目标拆分、公开命名、Node property、Theme 和 Binding 边界不得阻碍该模型。Ant Design 参考版本升级必须使用独立 change。

备选方案包括完整 Compose-style 公开 API、自创组件体系和直接模仿 Ant Design React Props。第一种会造成 `Modifier` 与 Token 冲突，第二种失去一致的企业级设计基线，第三种会把 DOM/CSS 字段带入原生 Runtime，因此都不采用。

### 9. DirtyFlags 显式映射到最小更新队列

每个可绑定属性在定义处固定 Dirty 映射：

- color、opacity -> `Material`
- translation -> `Transform` + `HitTest`
- size、padding -> `Measure` + `Layout` + `Geometry`

Scheduler 分别维护 Layout roots、Primitive updates 和 frame request。Material/Transform 更新直接写现有 `QuadPrimitive` 和对应 CPU-side instance range；只有 Geometry 或结构变化才重建相关 Primitive 数据。

如果一个更新同时产生多个 flags，采用包含其全部正确性的最小上界，但不得无条件提升为全树 Layout 或 Scene rebuild。

### 10. 示例与可观测性属于完成条件

最小示例使用可点击/定时切换的彩色 Quad，不依赖尚未实现的 Text。窗口标题或控制台输出以下计数：

- Component mount/run
- Signal write 和 Observer execution
- Measure 与 Layout node
- Primitive rebuild 与 instance update
- GPU upload、submit 和 idle wake

自动测试断言计数关系，真实窗口验收确认视觉变化、关闭流程和闲置行为。日志计数是诊断接口，不在此阶段承诺稳定格式。

## Risks / Trade-offs

- [vendored SDL3 与 `SDL_shadercross` 增加首次 clone/build 体积] -> 固定 revision、记录 license，并让 CI 缓存构建产物；system package 仅作为显式选项。
- [DXIL/SPIR-V 工具链在开发机缺失] -> configure 阶段 fail-fast，输出精确安装/路径说明，不在运行时静默降级。
- [共享持有 SignalCell 可能产生额外原子开销] -> 只在对象复制/销毁时发生；先用 benchmark 量化，再决定是否改为 intrusive reference count。
- [Observer 销毁与 dirty queue 交错会产生悬空访问] -> dirty queue 保存 generation-checked handle，Scope 销毁同时使 queued entry 失效。
- [Effect 自触发可能形成无限循环] -> epoch 隔离、最大轮数保护和依赖链诊断。
- [最小化窗口时没有 swapchain texture] -> 视为正常无可绘制状态，不创建 render pass，不报告 GPU 故障。
- [真实窗口行为难以完全自动化] -> 单元测试覆盖确定逻辑，Windows/Linux 验收记录保留实际 GPU driver、退出码和计数摘要。

## Migration Plan

这是空仓库上的新增能力，不需要数据迁移。实现按 tasks.md 的小阶段推进，每个阶段通过测试和 Git commit 固化：

1. 建立无 SDL3 依赖的 C++/CTest 基线。
2. 接入 SDL3 Window/GPU 与 shader 构建。
3. 完成 Reactive Runtime。
4. 完成 Node/Layout/Dirty/Quad 闭环。
5. 完成真实窗口与双平台验收。

任一阶段失败时回退该阶段提交即可；前序已验证层不应依赖后序未完成模块。
