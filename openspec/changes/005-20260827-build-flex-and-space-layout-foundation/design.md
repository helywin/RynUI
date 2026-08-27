## Context

见 `proposal.md` 的动机与范围。当前 `LayoutEngine` 已有 retained Node、Constraints、外部 width/height/min/max/margin、简单 horizontal/vertical `FlexLayout` 与 Button 专用 content layout，但现有 Flex 只做顺序累加和固定单值 gap，没有公开容器 API、wrap、justify、align、flex grow/shrink/basis 或 line 数据。ComponentHost 已能以 typed content closure 挂载稳定 child，paint traversal、HitTest 和 focus order均基于声明树。

本 change 横跨 public API、Component mount、LayoutStyle adapter、LayoutEngine、Default Theme、Dirty pipeline、示例和平台验收。布局运算必须使用 logical coordinates，不按显示器分辨率硬编码；普通 Props 更新不能重跑 content；layout order 的变化也不能偷偷改变 paint/focus 语义。

## Goals / Non-Goals

**Goals:**

- 发布能组合现有 Text、Button 和嵌套容器的 typed `ryn::Flex`/`ryn::Space`。
- 把单行累加布局提升为可测试的 line formation、grow/shrink、justify、align 和 wrap 算法。
- 使 gap、方向、对齐和 flex child 字段保持细粒度响应与 generation-checked 生命周期。
- 保留平台无关 layout contract，并把 Linux、Windows 真实窗口证据严格分开。

**Non-Goals:**

- 不实现 Grid、Scroll/Clip、reverse/RTL、baseline/align-content、separator/Compact 或结构响应。
- 不发布 DOM/CSS 兼容层、任意字符串值、通用 `Modifier` 或 Theme override。
- 不在本 change 改变 Button/Text 的稳定视觉 API、paint order 或 keyboard focus order。

## Decisions

### 1. Flex 与 Space 使用独立 typed Props，共享受控布局值

公开值包含 `FlexJustify`、`FlexAlign`、`SpaceAlign`、`SpaceSize` 和可表达 preset/custom 双轴数值的 `LayoutGap`。`FlexProps`/`SpaceProps` 的字段内部统一保存为 `Prop<T>`；builder 可以为 `SpaceSize` 和单值 logical gap 提供不损失语义的 convenience overload，但所有入口最终归一到 `LayoutGap`。方向使用与架构示例一致的 `vertical(Prop<bool>)`，默认 horizontal。

`LayoutStyle` 增加 `flex_grow(Prop<float>)`、`flex_shrink(Prop<float>)`、`flex_basis(Prop<LogicalLength>)`、`align_self(Prop<FlexAlignSelf>)` 和 `order(Prop<int>)`。它们仍是 child 相对父容器的外部布局字段，不进入组件 token 或 renderer。

备选方案是暴露 CSS 字符串或一个通用 flex shorthand。字符串无法在编译期限制支持范围，shorthand 会把解析、错误恢复和后续兼容固化进首批 API，因此不采用。

### 2. LayoutEngine 保存结构稳定的 Flex model 与可复用 line scratch

内部 `FlexLayout` 扩展为 direction、wrap、justify、align、main gap 和 cross gap；每个 Node 的 `ExternalLayoutStyle` 保存 flex item 字段。一次 layout 使用容器拥有的 scratch vectors 保存排序后的 child index、item hypothetical size、line 边界和分配结果；child 数量稳定后的重复 Measure/Layout 复用容量。

layout order 按 `(order, declaration ordinal)` 稳定排序，但 Node 树、Component paint traversal、HitTest paint order 与 FocusManager 声明顺序不重排。首批 margin 为非负，因此 order 改变不会制造重叠 z-order 歧义。

备选方案是响应式 order 更新时重排 Node children。那会把纯布局属性扩大成 Structure dirty，并改变 paint/focus identity，因此不采用。

### 3. Flex 每条 line 分三步确定主轴尺寸

第一步生成 hypothetical item：在扣除 padding/gap 后，以显式 finite basis 或 child intrinsic main size作为基准，并应用 width/height/min/max/margin。wrap 使用有限主轴上限做 greedy line break；单个超限 child 独占一行。

第二步在每条 line 内分配自由空间。正自由空间按 grow 权重分配；负自由空间按 `shrink * hypothetical main size` 权重分配。触及 min/max 的 item 冻结后重新分配剩余空间，直到所有 item 稳定或没有有效权重。浮点余数确定地交给布局顺序中的最后一个仍可调整 item，避免每帧边界抖动。

第三步用最终 main constraint 重新测量需要换行的 Text 或 stretch child，形成 line cross size，再执行 justify 与 align。无限主轴约束下不 wrap，也不分配无限自由空间；没有 child 的容器只返回 padding/外部约束决定的尺寸。

备选方案是只在 place 阶段缩放 bounds。它不能让 Text 按最终宽度重新换行，也无法正确应用 min/max，因此不采用。

### 4. Justify、align 与 gap 使用 logical coordinate 确定放置

`start/center/end` 从可用自由空间计算起点；`space-between/space-around/space-evenly` 只在正自由空间时增加动态间隔，基础 main gap 始终保留。单 item 的 `space-between` 等价 start，`space-around/space-evenly` 仍按各自边缘份额计算。

cross axis 的 start/center/end 只改变 placement；stretch 仅作用于交叉轴为 auto 的 item，并在显式 min/max 内固定重新测量。`align_self` 覆盖容器 align。wrap 后 line 以 cross gap 顺序排列；首批不提供 align-content，容器额外交叉空间保留在末端。

所有数值都是 logical pixels。SDL drawable pixel size 与 display scale 仍由 renderer/platform 边界处理，layout preset 不因窗口移动到另一输出而改变。

### 5. Space 是受限 Flex policy，不创建公开或内部 wrapper Node

Space 解析为禁止 grow/shrink/basis/order 生效的顺序布局 policy：horizontal/vertical、可选 wrap、start/center/end align，以及 preset/custom main/cross gap。direct child Node 自身就是 item identity；由于首批不支持 empty placeholder、separator 或 Compact，不需要为每个 child 增加 wrapper Node。

这样能保持 Component tree、paint traversal、HitTest 和资源数量最小，同时兑现可见间距语义。未来 separator/Compact 若需要 item wrapper，必须由新 change 定义 identity、可访问性和 scene order 迁移，不能在本阶段预留不可见 public handle。

备选方案是仿照 DOM 为每个 child 无条件建立 wrapper。它会扩大 Node、layout、HitTest 和 scene 映射，却不给首批可观测合同带来价值，因此不采用。

### 6. Gap preset 从 Default Theme snapshot 单点解析

Default Theme 增加共享 layout spacing token，Small/Middle/Large 固定为 Ant Design 6.5.0 的 `paddingXS`/`padding`/`paddingLG`，即 8/16/24 logical pixels。Flex 与 Space 读取同一 snapshot；custom gap 绕过 preset 解析但仍经过非负有限校验。

参考边界：

- [Flex API](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/flex/interface.ts)
- [Flex gap 与布局状态](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/flex/style/index.ts)
- [Space API 与默认 small size](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/space/index.tsx)
- [Space 8/16/24 preset 派生](https://raw.githubusercontent.com/ant-design/ant-design/6.5.0/components/space/style/index.ts)

RynUI 只映射布局行为和值，不引入 React、DOM 或 CSS-in-JS 依赖。

### 7. Component adapter 只拥有容器 root 与响应连接

Flex/Space mount 各创建一个无视觉 root Node，连接 public `LayoutStyle`，执行一次 content closure，并把 direct children 保留在声明树。每个 Props binding 在成功验证新值后更新 root layout model；异常更新不得覆盖上一份 model。cleanup 先停止 child/Props Scope，再释放 layout scratch 与 root Node。

gap/justify/align/order 变化不重建 component/scene topology。justify/align 在容器和 child measure revision未变时允许只重放 Place；gap/direction/wrap/grow/shrink/basis 必须请求目标容器子树 Measure/Layout，随后同步 Geometry/HitTest。实现以诊断计数和 sibling isolation tests 证明最小范围。

备选方案是把 Flex/Space 写成每次状态变化重新执行的 convenience function。它违反 retained identity，并会让 Button capture/focus 在布局更新时丢失，因此不采用。

### 8. 示例与验收先证明响应式页面布局，不宣称 Grid/Scroll

新增公开 layout 示例，组合嵌套 Flex、Space、Text 与 Button，并由 Button 切换 direction、wrap、justify、align、gap 和 item grow/order。headless frame tests 在多个 viewport width 下锁定 bounds、line break、dirty/count 与 idle；真实窗口截图覆盖宽/窄两种布局和交互更新。

正式 build 继续使用 `CMakePresets.json` 与 Ninja Multi-Config：Linux 使用 `linux-gcc`/`linux-clang`，Windows 仅使用 `windows-msvc`。evidence 文件、截图和 checklist 分平台保存；本机覆盖只进入未提交的 `CMakeUserPresets.json`。

SDL window 使用 `SDL_WINDOW_HIGH_PIXEL_DENSITY`。平台边界同时保存 window coordinate size、drawable pixel size、pixel density 与 display scale；RynUI logical viewport 计算为 `drawable pixels / display scale`，SDL mouse/touch coordinate 计算为 `window coordinate * pixel density / display scale`。这样 LayoutEngine、Theme token 与 `ryn::dp` 继续使用不随显示器改变的 logical value，GPU clip-space 归一化则自然把 logical geometry 映射到实际 swapchain。`SDL_EVENT_WINDOW_RESIZED`、`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` 与 `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` 都重新查询 metrics 并发布 logical resize，避免窗口跨输出后保留旧比例。

### 9. 字体布局尺寸与字形栅格密度分离

`FontRasterConfig` 显式携带 logical pixel size 与启动时窗口的 display scale。FreeType raster face 使用 `ceil(logical_pixel_size * display_scale)` 的实际 raster pixel size 生成灰度 coverage；独立 shaping face 保持 logical size，避免 hb-ft 调整 shaping scale 时把 raster face 退回低密度。`FontMetrics` 同时报告 logical size、实际 raster size 与由整数栅格尺寸得到的 effective raster scale。HarfBuzz 输出固定在 logical 26.6 单位，Text measure 不接收物理 raster pixel；Glyph Atlas key 使用实际 raster size，entry 保存 raster scale，Glyph Scene 把物理 bearing/coverage 除以该 scale 后再生成 logical quad。每个 coverage 周围的一像素透明 padding 同时进入 UV 与 quad，给 linear sampler 留出由 coverage 过渡到零的 guard，避免字母边缘在紧贴 bitmap 边界时呈现裁切感。这样 150% 缩放时 14 logical pixel 字体使用 21px coverage，而文字流、Button 高度和换行仍服从原有 logical layout。

首批在字体载入时绑定当前窗口 display scale；窗口移动到其他输出后，platform viewport 与 input 会立即刷新，但字体重新载入及跨 density atlas 淘汰需要后续 change 定义稳定 font resource identity 和 TextState 刷新合同。本 change 的 Windows 清晰度验收必须在目标 display scale 下重新启动示例，不能把启动后跨输出仍沿用旧 atlas 描述为已支持。

默认 UI font chain 不再把 validation font 当作首选视觉字体。Windows 通过 DirectWrite system collection 按 `Segoe UI Variable Text`、`Segoe UI Variable`、`Segoe UI` 的顺序选择 Latin UI face，并用 `Microsoft YaHei UI` 补足简体中文；Linux 通过 Fontconfig 的 `sans-serif` generic family 分别按 `en` 与 `zh-cn` 匹配 `FC_FILE`、`FC_INDEX` 和 `FC_FAMILY`。Linux 构建以 `find_package(Fontconfig 2.13 REQUIRED)` 显式接入平台服务，不执行命令行 `fc-match`，也不提供静默 system-first 构建回退。

`DefaultFontChainRequest::preferred_fonts` 保留应用配置其他字体文件与 face index 的 typed 边界。加载顺序为 explicit custom、缺失 coverage 对应的 platform system face、locked validation fallback；显式 custom 文件不可读时 fail-fast，不能悄悄忽略用户配置。示例输出 `font_source` 与 `font_families` 便于真实窗口对照；确定性 headless 字体测量仍直接使用锁定 validation font，避免不同桌面配置改变基线。Theme font token 尚未发布，因此该 request 保持内部 integration API，后续公开 Theme change 复用此顺序而不暴露 DirectWrite/Fontconfig 类型。

## Risks / Trade-offs

- **[flex freeze/redistribute 的浮点误差导致边界抖动]** → 使用稳定排序、有限迭代、epsilon 和确定余数接收者，并用 fractional constraints 重复运行测试锁定结果。
- **[Text 在最终主轴约束下重测形成反馈]** → line formation 以 hypothetical size 建立，最终重测只更新 line cross size；对同一 constraints 使用既有 intrinsic cache，并限制一次 layout 的阶段数。
- **[wrap 大量 child 的 scratch 分配]** → scratch 由容器 slot 持有并复用，增加稳定 child count allocation benchmark；结构增长允许扩容。
- **[order 与 keyboard focus 顺序不同令用户困惑]** → 明确 order 只控制 layout；示例避免把交互流程依赖于视觉重排，未来若需要 visual-order navigation 由 Accessibility/focus policy change 决定。
- **[Space 不建 wrapper 限制 separator/Compact]** → 将两者明确排除；未来 change 可以引入受控 item layer，不伪装首批已支持。
- **[Windows 与 Linux 字体度量或像素舍入不同]** → 自动测试锁定 logical bounds，真实窗口证据各自保存；任何平台结果不外推另一平台。
- **[fractional scale 的整数 raster size 引入度量舍入]** → raster size 向上取整并记录 effective scale；HarfBuzz 保持 logical scale，Glyph Scene 使用同一个 effective scale 折回 bearing/coverage，自动测试锁定 logical layout 与高密度 coverage 的分离。
- **[Glyph quad 紧贴 coverage 令 linear filter 呈现边缘裁切]** → atlas 保留并上传透明 padding，UV 与 quad 一起覆盖 guard pixel；场景按 effective scale 折回 padding，自动测试锁定透明边界和 geometry。
- **[窗口跨不同 scale 输出后旧字体 atlas 仍在使用]** → 首批明确为 load-time density；真实验收在目标输出启动示例，动态 font resource/atlas 切换由后续 change 定义，不以 viewport 已刷新冒充字体已重栅格化。
- **[系统字体配置因用户和发行版不同而改变视觉与度量]** → 示例与真实窗口尊重平台默认并记录 family/source，确定性 headless tests 继续使用锁定字体；Windows 与 Linux 证据互不外推。
- **[显式 custom 字体失效后静默替换造成品牌视觉漂移]** → custom 文件加载失败即返回诊断；只有成功载入但 coverage 不足时才进入系统和 bundled fallback。

## Migration Plan

1. 扩展内部 layout value、Flex model 与算法，保持现有简单 Flex 和 Button content tests 兼容。
2. 扩展 LayoutStyle flex child 字段和 Default Theme gap token，不改变现有 consumer 默认值。
3. 增加 Flex/Space public adapter、header 与 contract tests，再迁移新示例使用公开 DSL。
4. 分别完成 Linux 与 Windows build、CTest、真实窗口和 evidence；失败时可移除未发布的新 headers/targets，现有 Text/Button API 无需迁移。
