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

### 1. Preset 统一平台和配置矩阵

仓库提交 `CMakePresets.json`，所有正式 configure、build 和 test 流程使用 `Ninja Multi-Config`。configure preset 至少包含 `windows-msvc`、`linux-gcc` 和 `linux-clang`，并通过 `condition` 只在匹配的 host 上启用；build/test preset 使用 `Debug` 或 `Release` configuration，不为每种配置复制 configure tree。

Windows preset 必须在 Visual Studio Developer Environment 中解析到 MSVC x64，并由根工程在 configure 阶段检查 `MSVC`，发现 MinGW、GCC 或 Clang 时立即失败。Linux preset 分别显式选择 GCC 与 Clang。共享 cache variables 放在 hidden base preset，本机 Visual Studio 路径、SDK 路径或其他个人覆盖只允许写入不提交的 `CMakeUserPresets.json`。

这套约束让 preset 成为平台构建合同，同时保持 Ninja 生成和构建行为一致。备选方案包括 Visual Studio generator 和 Windows 上的 MinGW；前者会造成 Windows/Linux 生成模型分叉，后者不满足 Windows/MSVC 验收要求，因此均不作为正式基线。

### 2. 工程目标按架构层拆分

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

### 3. 第三方依赖使用显式 BUNDLED 或 SYSTEM 模式

根工程提供 cache string `RYNUI_DEPENDENCY_MODE`，只接受 `BUNDLED` 与 `SYSTEM`，不提供会因机器环境而选择不同版本的 `AUTO`。所有依赖解析集中在 `cmake/dependencies/`，版本、不可变 source URL、SHA256、license 标识和规范 target 记录在同一 lock 文件；第三方配置不得散落到业务 targets。

`BUNDLED` 模式使用 `FetchContent` 获取固定 SDL3 release archive，并通过 `URL_HASH SHA256=...` 验证内容。离线或镜像环境可使用 CMake 标准 `FETCHCONTENT_SOURCE_DIR_SDL3` 指向预先准备的源码；仓库不提交 SDL3 Git submodule，也不在 configure 时跟随分支或解析 latest release。

`SYSTEM` 模式只调用 `find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3)`。vcpkg、Conan 和 Linux 发行版 package 均通过向该模式提供 CMake package 参与，不成为 RynUI 强制依赖管理器。两种模式必须产生 SDL3 官方规范 target `SDL3::SDL3`，平台层之外的 target 不得直接链接它。

默认模式为 `BUNDLED`，保证开发与 CI 使用相同来源；发行版集成和已有 superbuild 显式选择 `SYSTEM`。备选方案包括 Git submodule、只支持 system package 和 system-first fallback：submodule 增加递归 clone 与工作树状态，system-only 不能提供一致的 Windows 基线，隐式 fallback 不能稳定复现，因此均不采用。

### 4. Shader 使用离线编译，不在应用运行时转换

Quad shader 的单一源文件使用 HLSL。构建阶段调用固定版本的 `SDL_shadercross` CLI，为本 change 生成 Windows/D3D12 所需的 DXIL 和 Linux/Vulkan 所需的 SPIR-V；生成物位于 build tree，不手工编辑。`SDL_shadercross` 的版本、source URL、SHA256 和 license 与 SDL3 一起进入依赖锁定记录。

`RYNUI_SHADERCROSS_EXECUTABLE` 可显式指定已安装或预构建的 host CLI；未指定时，原生 `BUNDLED` 构建可以从锁定源码生成 host tool。交叉编译不得执行 target binary，必须显式提供 host executable。配置阶段检查工具可执行性并给出可执行的缺失依赖错误。应用运行时只加载与当前 SDL GPU driver 匹配的已编译 shader，不链接 `SDL3_shadercross` 运行时转换库。macOS/MSL 输出留给后续 change。

备选方案包括运行时调用 `SDL_shadercross` 和分别维护多份 shader 源码。前者增加部署依赖与启动失败面，后者容易发生语义漂移，因此不采用。

### 5. 平台对象使用显式 RAII 状态机

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

### 6. Reactive Runtime 先保证生命周期正确，再替换存储优化

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

### 7. Node 使用 generation handle，Component 只负责首次挂载

Node 存放在拥有稳定 slot 的容器中，以 `{index, generation}` 形式对外引用。删除后增加 generation，旧 handle 必须被诊断为失效，不得访问复用后的 Node。

Component mount 创建 `Scope` 和 Node；普通 Binding 更新只修改 Node property。首个 change 不实现 `If`、`For` 和 subtree reconciliation，因此 Node 结构在挂载完成后保持不变。

备选方案是让公开 API 暴露裸 Node 指针。该方案无法可靠诊断销毁后的访问，也会阻碍未来结构响应，因此不采用。

### 8. Layout Engine 只实现内部 Box 与 Flex 核心

`Constraints` 在入口验证 `min <= max`。Measure 返回 size，Place 写入最终 bounds；每个 Layout pass 使用递增 generation，测试可以读取节点的 measure/place 次数。

首个闭环内部只支持 `BoxLayout`、可切换 horizontal/vertical 的 `FlexLayout` 以及固定尺寸、fill、padding 和 gap 所需数据。公开布局层未来按照 Ant Design 6 提供 `Flex`、`Space`、`Grid`（`Row`/`Col`）和 `Layout`；本 change 不把内部 Layout 类型承诺为稳定公开 API。Grid、Scroll、intrinsic measurement 和 baseline alignment 不进入本 change。

### 9. Ant Design 决定公开组件合同，Compose 只提供机制参考

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

### 10. DirtyFlags 显式映射到最小更新队列

每个可绑定属性在定义处固定 Dirty 映射：

- color、opacity -> `Material`
- translation -> `Transform` + `HitTest`
- size、padding -> `Measure` + `Layout` + `Geometry`

Scheduler 分别维护 Layout roots、Primitive updates 和 frame request。Material/Transform 更新直接写现有 `QuadPrimitive` 和对应 CPU-side instance range；只有 Geometry 或结构变化才重建相关 Primitive 数据。

如果一个更新同时产生多个 flags，采用包含其全部正确性的最小上界，但不得无条件提升为全树 Layout 或 Scene rebuild。

### 11. 示例与可观测性属于完成条件

最小示例使用可点击/定时切换的彩色 Quad，不依赖尚未实现的 Text。窗口标题或控制台输出以下计数：

- Component mount/run
- Signal write 和 Observer execution
- Measure 与 Layout node
- Primitive rebuild 与 instance update
- GPU upload、submit 和 idle wake

自动测试断言计数关系，真实窗口验收确认视觉变化、关闭流程和闲置行为。日志计数是诊断接口，不在此阶段承诺稳定格式。

## Risks / Trade-offs

- [BUNDLED SDL3 与 `SDL_shadercross` 增加首次下载/build 体积] -> 固定 archive 与 SHA256、记录 license、允许标准 source override，并让 CI 缓存下载和构建产物；`SYSTEM` 仅作为显式模式。
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
