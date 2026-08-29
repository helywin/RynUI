## Context

见 `proposal.md` 的动机与范围。当前 `rynui_token_gallery` 用 51 个 `ryn::Button` 同时承担 Theme 切换、状态样例、色板、尺寸、圆角和阴影展示；因此纯说明单元也进入 InteractionRegistry、focus order 与 Button hover resolver。Gallery 只有一个 wrap Flex，没有文档 section、来源版本、完整组件 catalog、支持状态或长内容导航。

Ant Design `6.5.0` tag 对应 commit `740ad964dc2397f33e40944367b0536a7314cc32`。其 Introduction 描述企业级设计系统、Natural/Certain/Meaningful/Growing 四项价值与资源/实现入口；组件 overview 由文档 frontmatter 生成七类 72 项清单。2026-08-29 的 live site 已显示 6.6.2，并包含 6.5.0 之后的条目，因此 live page 不能作为无版本运行时数据源。

现有 retained Runtime 已有 Flex/Space、Button/Text、Theme、paint traversal、HitTest、focus、translation、ancestor clip 与按需帧，但没有公开 Scroll、Card、Input 或文档 shell。这个 change 必须让 Gallery 可用，同时避免借示例偷偷固化尚未设计完成的公共 API。

## Goals / Non-Goals

**Goals:**

- 用版本化离线 catalog 固定 Introduction 摘要、来源、七类 72 项组件与 RynUI support status。
- 建立文档式 Header、Navigation、Introduction/Foundation、Component Overview 与 live sample 区。
- 让非交互 reference surface 不进入 Button/Focus/Click 语义，真实控件样例继续使用公开 RynUI 组件。
- 提供长内容 viewport、section anchor、pointer wheel、类别/状态导航、宽窄 reflow 与 retained 最小失效。
- 通过 Gallery 暴露并修正 Button hover/focus/solid geometry 的真实视觉差异。

**Non-Goals:**

- 不实现 catalog 中尚未存在的全部 Ant Design 组件，也不以 placeholder 宣称实现。
- 不发布稳定公共 Scroll/Card/Input/Application API；Gallery 专用基础设施保持 internal。
- 不支持 HTML、Markdown runtime、浏览器 CSS、远程图片、运行时网络请求或全文检索输入框。
- 不升级 Design Token baseline，不复制 Ant Design React implementation，也不逐字镜像官方文档。

## Decisions

### 1. Catalog 由锁定 source manifest 生成并提交，不在运行时抓取

新增机器可读 source manifest，记录 schema、Ant Design version/commit、Introduction source path、category order 和每个 component 的 stable id/name/category/source path。生成器只把最小元数据编译为 C++ table，并生成 catalog hash；中文摘要、RynUI status、scope 与 evidence pointer 由仓库内 reviewable overlay 提供。

生成输入和输出都进入 Git，CI/CTest 用 `--check` 模式证明输出新鲜、七类计数为 4/7/7/18/20/11/5、总数为 72、identity 唯一且 source path 指向 6.5.0 snapshot。生成器不得下载上游；升级者必须显式准备并审查新 manifest。Python 运行使用 `-B` 或 `PYTHONDONTWRITEBYTECODE=1`，仓库 contract 同时拒绝 `__pycache__`、`.pyc` 与 `.pyo`。

备选方案是启动时读取 ant.design 或 GitHub。它会让离线构建、可重复验收、版权边界和版本锁失效，因此不采用。备选方案是手写多份 C++ 数组；它容易使导航、计数、文档与测试漂移，因此采用单一 manifest + overlay。

### 2. Information architecture 固定为文档 section，不再是 Token 按钮矩阵

Gallery document 顺序为 Header/Source、Introduction、Design Values、Foundation/Token、Component Overview、Live Samples。Component Overview 内按 General、Layout、Navigation、Data Entry、Data Display、Feedback、Other 排列，每个 category 和 entry 都持有稳定 identity。左侧或窄窗口顶部 Navigation 从同一 catalog 派生，支持 category 与 support status 筛选；不提供尚无 Input/IME contract 的自由文本搜索。

宽窗口使用 navigation + document 双栏，窄窗口将 navigation reflow 为顶部 wrap controls，document 保持单列或低列数卡片。category anchor 与当前 support filter 是 reactive state；更新不得重建 catalog 或重跑无关 live sample content。

备选方案是一次显示 72 个极小 Button。它虽然能塞进窗口，却不可读、语义错误且不能承载 scope/evidence，因此不采用。

### 3. ReferenceSurface 是 Gallery internal retained component

新增 Gallery internal `ReferenceSurfaceProps` 与 typed content slot，负责背景、边框、圆角、状态 badge、可选 swatch 和外部 `LayoutStyle`。其视觉只读取 Theme/Reference Component Token，并复用 retained Quad/RoundedEffect 与 Text scene；默认不注册 InteractionId，不可 focus、hover、pressed 或 click。只有显式 NavigationControl 和 live sample 使用 `ryn::Button`。

`ReferenceSurface` 不从 `rynui.hpp` 导出，命名和 ABI 不属于公共承诺。实现仍使用 typed Props/slot 与 component lifecycle，避免内部 imperative scene 绘制绕过 retained identity。未来正式 `ryn::Card` change 可以迁移其视觉合同，但不能把当前 internal type 当作公共 Card。

备选方案是把 reference item 设成 disabled Button。disabled 会替换颜色、隐藏 shadow、污染组件语义，也无法正确展示 Token，因此不采用。备选方案是给 Button 增加 `interactive(false)`，这会创造语义自相矛盾的公共 API，因此不采用。

### 4. 长文档 viewport 先保持 internal，并复用 Node translation/clip

`GalleryDocumentViewport` 保存 logical scroll offset、content extent、viewport extent 与 section anchors。内部 root 设置 viewport clip，document subtree 使用 generation-checked translation；scroll 变化只更新目标 subtree Geometry/HitTest 和 navigation current-section，不重新 Measure 未变化内容。Layout 或 resize 改变 content extent 后 clamp offset，并以 section identity 恢复最近 anchor。

SDL adapter 增加平台无关 wheel delta，Gallery runtime 消费 vertical wheel；类别 Navigation Button 可直接跳到 anchor，因此 keyboard-only 用户不依赖 wheel。`Tab` order 只遍历 navigation/filter/live samples。首批不发布通用 overscroll、momentum、touch pan、scrollbar styling 或 public Scroll handle。

备选方案是分页销毁/重建 category content。它会破坏 stable identity、状态与 retained diagnostics，也不符合网页式连续浏览，因此不采用。

### 5. Support status 使用显式 overlay 和 evidence gate

每个 entry overlay 保存 typed `GallerySupportStatus`、中文摘要、已支持 scope、缺失 scope 与 evidence identifiers。状态语义固定：`implemented` 为声明的 RynUI mapping scope 已实现并验收；`partial` 为存在可用子集但不覆盖 Gallery 列出的主要 Ant 能力；`planned` 只有规划；`web-only` 为不适用的浏览器合同；`deprecated` 跟随锁定快照；`out-of-scope` 为明确不做。

初始 overlay 对 Text/Typography、Button、Flex、Space 与 Theme/ConfigProvider 使用 `partial`，因为当前 RynUI 只实现受控子集；其余条目按现有代码/OpenSpec 证据标记。contract 要求 `implemented`/`partial` 有非空 scope 与可解析 evidence，避免通过编辑 label 夸大完成度。

备选方案是只显示 implemented/pending 二值。它无法区分已有子集、平台不适用与明确排除，会继续诱发错误验收，因此不采用。

### 6. Button audit 保留正确 hover 语义并修复真正偏差

Ant Design 6.5.0 的 Default outlined Button hover 确实把现有 1px border 和文字切到 `defaultHoverBorderColor`/`defaultHoverColor`；这不是外部 focus ring。`genFocusStyle` 只在 `:focus-visible` 使用 `lineWidthFocus = lineWidth * 3`、`colorPrimaryBorder` 与 offset 1。RynUI 保留这些值，并增加测试证明 hover 时 outline opacity 为零。

Primary/Danger solid 的 CSS border 为 transparent，但 background painting 覆盖 border box。RynUI 当前始终把 background inset 1px，导致 solid fill 留出透明边缘；实现应让 solid background 覆盖完整 root bounds，同时保持 HitTest 和 layout 尺寸不变。Danger hover/active 使用锁定 palette `colorErrorHover`/`colorErrorActive`，不得用通用 white/black mix 近似。自定义色板由非交互 swatch 展示，不再通过只覆盖 normal background 的 Button Theme 造成 hover 回退蓝色。

已知桌面适配继续保留：loading indicator 暂为静态、motion/wave/icon/更多 variants 不在本 change 扩展；Gallery 的 Button scope 文本必须如实标记这些差异。

### 7. Gallery 内容只做摘要与引用

Introduction 只保存 RynUI 编写的简体中文摘要、四项设计价值名称、分组标题和官方 URL；不复制整页段落、社区评价、用户名单或远程品牌图片。Component catalog 保存名称、类别、简短自有摘要与 source path，不复制每个组件 API 文档或 demo source。

这样既能提供用户要求的设计/组件全景，又保持 Gallery 是 RynUI 的实现参考，而不是 Ant Design 网站镜像。

### 8. 验收按平台通用与平台专属拆分

catalog counts/hash、status/evidence、reference semantics、Button state、scroll math、reflow、retained identity 和 source contracts 属于平台通用任务，只在一个受支持平台运行一次。Windows 与 Linux 分别验证真实 window system、DPI、system font、GPU/shader、wheel/keyboard 输入、clip、Button focus 与长内容 reachability；平台 checkbox 和 evidence 独立。

正式构建继续由 `windows-msvc`、`linux-gcc`、`linux-clang` presets 与 Ninja Multi-Config 驱动。本机路径只允许放在 ignored `CMakeUserPresets.json`；不新增依赖模式或网络步骤。

## Risks / Trade-offs

- **[72 项 metadata 与官网未来版本漂移]** → catalog 显示 version/commit/hash，生成器固定 category/count contract，升级必须独立 change。
- **[Gallery internal surface 演变成第二套组件库]** → 只实现文档展示必需字段，不导出 public header；公开 Card/Scroll 另建 capability。
- **[长文档 clip/translation 使 HitTest 漂移]** → scroll 使用 committed translation/effective clip，同帧同步 Geometry/HitTest，并覆盖越界和 resize tests。
- **[状态 overlay 过期]** → evidence id contract、OpenSpec task review 与 Gallery visible source/status 共同约束；禁止 planning-only 标记 implemented。
- **[一次挂载全部 section 增加资源]** → 72 项规模保持完整 retained tree，复用 atlas/glyph/quad capacity；以 benchmark 与 idle counters 设定上限，暂不引入 virtualization。
- **[用户把 Default hover 蓝边误认为 focus ring]** → Gallery 分开显示 hover/focus sample、标注 token 名，并确保 non-interactive content 不触发任一状态。
- **[官网摘要带来版权或品牌混淆]** → 仅使用自有摘要和短名称，显示“Ant Design reference / RynUI implementation”与官方链接，不打包远程图片和原文镜像。

## Migration Plan

1. 先引入 catalog source/overlay、generator、hash 与纯数据 contract，不改变现有 Gallery executable 行为。
2. 修复 Button solid geometry/Danger palette，并完成平台通用状态矩阵；独立提交，不混入 Gallery shell。
3. 增加 internal ReferenceSurface 与 document section model，迁移非交互 Token/目录内容，保留现有 live Button samples。
4. 增加 document viewport、wheel/anchor navigation、support filter 与宽窄 reflow，再接入全部 72 项 catalog。
5. 更新 headless/benchmark/evidence schema，分别完成 Windows 和 Linux 真实窗口验收。

每一步都能以独立 commit 回退。旧 Gallery 没有外部数据格式或公共 API；迁移失败时可保留旧 executable，同时 catalog 和 Button 修复继续独立存在。
