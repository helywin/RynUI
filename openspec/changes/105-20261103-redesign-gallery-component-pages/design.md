# Design

## Context

动机见 [proposal.md](proposal.md)。本轮设计由 Astra xhigh 完成，基于 `63479dd98f0d744efc3780027b78047433f424c5` 源码，只生成规划和 HTML 设计示意，未运行原生程序。聊天内 HTML 预览用于审阅设计，不作为原生实现或平台验收证据。

### 现场事实

| 位置 | 已观察到的行为 | 对方案的约束 |
| --- | --- | --- |
| `token_gallery_definition.cpp::make_token_gallery_definition` | 单一 `GalleryState`，同一正文 closure 依次创建 source、所有 overview entry、全部 `add_*_samples` | 必须拆分实际构造边界，仅隐藏/裁剪整页不能消除所有挂载成本 |
| `gallery_document_model.*` | 六个固定 section，七类状态筛选，component 导航 identity | 保留 canonical identity，分离页面选择和页内章节 |
| `gallery_document_viewport.*` | 六 section/七 category 固定锚点；组件导航用 identity 条件链扫描 Text 文案选择样例锚点 | 新锚点必须由当前页/样例登记显式提供，不靠正文字符串和全目录 Text 扫描 |
| `token_gallery_runtime.cpp::GalleryEvents/Submitter` | 导航延迟至布局后跳转；scroll 优先交给 inputs；scrollbar 用 mounted surface 末尾索引定位 | 保留嵌套滚轮路由，替换全局序号依赖；切页应有明确事件边界 |
| `ReferenceSurfaceHost` | 有 mount/destroy，但没有建立 ActiveReferenceSurfaceHost 上下文的 append_slot 入口 | 页面增量挂载需要小型 Gallery 内部 adapter；不能假设公开动态页面 API 已存在 |
| `WindowComponentServices` / `runtime::ComponentHost` | 已有 `append_slot(parent, Content)`、`destroy`、scope cleanup、generation 和输入参与者清理 | 可复用内部能力实现单个页面宿主，不需 VDOM 或公开 Router |
| `tests/token_gallery_frame_tests.cpp` | 458 个 stable test ID，固定全页 307 live samples 与多处 mounted 索引、文案前缀查找 | 这些是当前源码中的断言，不是本轮运行结果；迁移必须保留行为覆盖，改为稳定登记定位 |

目录权威来源是 `gallery/ant-design/6.6.5/source-manifest.json` + `support-overlay.json`，经 `tools/update_ant_design_gallery_catalog.py` 生成 `generated_ant_design_reference_catalog.inc`，由 `AntDesignReferenceEntry` 暴露。锁定 commit 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。本轮统计为：通用 4、布局 7、导航 7、数据录入 18、数据展示 21、反馈 11、其他 5；implemented 11、partial 39、planned 22、deprecated 1、web-only 0、out-of-scope 0。统计是元数据快照，后续 UI 从数组计算，不复制数字常量。

只读参考 [Ant Design 总览](https://ant.design/components/overview-cn/) 和 [Button 文档](https://ant.design/components/button-cn/) 的组件目录、用途/示例/API/Design Token 层级；官网实时版本不作为运行时数据源。保留仓库 6.6.5，不复制官网大段文案、图像或 React 代码。

### 与既有规划的关系

008 的“固定长文档”以及 032 的“窄窗口导航与正文连续滚动”在本 change 中由独立页与窄目录面板替代。保留其离线来源、非交互说明、独立滚动和 retained 原则。008 早期版本/状态已被后续变更演进，本次按当前 catalog，不恢复 6.5.0/72 项或早期 Button partial。033 的组件 identity 跳转改为 page route。100 的离屏 glyph、translation 和 dirty/upload 优化继续使用，但不把裁剪等同于按页卸载；其旧人工完成门槛不继承。104 的字体、clip、视觉与 cursor 修复保持共用路径，本 change 不重新实现它们，也不把 Windows 结果外推到 Linux。

当前主 `openspec/specs/` 没有 capability，因此新增 `gallery-component-pages`；上述冲突明确写在这里，历史 change 不改写。

## Goals / Non-Goals

**Goals:** 一眼确认正在看哪个组件、可体验什么、与官网有哪些能力差异；通过结构性页面边界降低无关工作；让已有真实样例与测试可持续维护。

**Non-Goals:** 新建 Web 产品、实现剩余组件、升级 Ant Design 基线、完整生成 C++ API 文档、公共导航框架、所有卡片的滚动虚拟化、跨启动持久化、整页 240 Hz 承诺、系统 IME/辅助技术人工验收。本轮不生产代码、不编译、不打开用户桌面窗口，仅提交规划文档。

## Decisions

### 1. 单一文档站方向：内容优先，边界就近呈现

视觉用浅色纸面、细分隔线、蓝色当前项、清晰字号和适量留白。页面不套大圆角外卡，不使用大渐变、仪表盘图表、巨型状态卡或操作日志。组件自己的 Card 是真实样例，不把所有文档区域做成层层卡片。

顶部 60 logical units：`RynUI` 字标、`组件`、`Design Token` 入口、参考版本、Default/Dark 主题与 Compact 密度控制。品牌色与 motion 控制收入可展开的“主题设置”，作用仍为全局主题，不混进每个组件样例。

左侧：组件搜索、状态筛选、总览与介绍入口、按七类展开的目录。名称英文为主、中文为辅，选中行蓝色浅底与 2-unit 起始边；状态使用小点配文字/可访问名称，不能只靠颜色。默认全部类别展开；搜索时只显示命中及其分类，分类折叠状态只保存在会话。总览保留可比较的状态列与短摘要，不承载全部 live sample。

组件正文顺序：

1. 面包屑/类别、`Button 按钮` 标题、原始状态徽标及简短用途；状态旁可见“按声明范围”。
2. `示例 / C++ 与 API / 支持范围 / Design Token` 页内导航。宽屏在右侧 TOC 展示；中窄屏是正文顶部的紧凑导航，不把它当组件页切换。
3. 示例卡：预览区 → 具名标题/一句说明 → 展开 C++。预览背景为 container，24-unit 内边距，至少 112-unit 高；内边距随窄屏降到 16。复杂编辑/表单卡横跨正文，只有小型且独立的样例才允许两列。卡底工具只保留代码展开/复制，不摆验收按钮。
4. C++ 与 API：当前家族入口、常用 typed Props/slots、公共头文件路径和可选打开来源；不是把 Web API 表机械翻译为 C++。
5. 支持范围：已支持、缺失/范围外与平台证据边界。原始 scope 文本完整可展开，避免仅显示经过润色的摘要。不能用“支持项 N/总项 M”推出百分比。
6. Design Token：当前组件相关 token 的名称/用途与有效主题值；仅当当前组件有真实 token 映射时提供，引用全局 token 页。来源折叠区列原始 identity、版本、路径、evidence identifiers。

无实现页保留标题、用途、状态、参考/缺失范围与来源，用简短说明代替预览区；Upload 不出现文件选择器，List 的 deprecated 不被改写为 planned。当前 web-only/out-of-scope 为零，但页面生成器覆盖全部六状态。已有组件尚未登记示例时明确写“本页尚无已登记样例”，不降低其 catalog 状态。

备选“每类一页”无法满足单组件查阅；备选“仍挂全部样例，只改目录锚点”没有页面隔离，均不采用。

### 2. 宽、中、窄窗口与尺寸依据

所有生产尺寸使用 logical units，通过同一 `GalleryLayoutMetrics` 供布局、clip、滚动条和 HitTest 使用；不按 SDL 物理像素另算一套。

| 窗口宽度 | 导航 | 正文 | TOC | 留白 |
| --- | --- | --- | --- | --- |
| ≥ 1200 | 240，独立滚动 | flex，阅读区域最大 920 | 160，右侧独立短栏 | 正文左右 40，栏间 24 |
| 640–1199 | 184–216，独立滚动 | 剩余宽度；728 时约 480 可用 | 正文上方页内导航 | 正文左右 24；紧张时 20 |
| < 640 | 顶部“组件目录”按钮打开 280 或可用宽度的面板 | 单列 | 正文上方可换行/可横向滚动的页内导航 | 正文左右 16 |

导航行 36、分类标题 12/20、正文 14/22、辅助说明 12/20、页标题 32/40（窄屏 28/36）、章节 22/30、示例标题 16/24、代码 12–13/20。正文标题与摘要间 12，页头后 28–32，章节间 32–40，示例间 16，表格单元 12×16。中/窄屏示例单列；正文实际可用宽度 ≥ 720 才允许两列，每卡至少 336。

顶栏在窄屏换成两行最多约 100 高，版本收入设置，保留主题与密度入口。窄目录有明确关闭按钮、Escape 关闭、Tab 循环和输入边界；可使用已存在的 `ryn::Drawer` 子集或 Gallery 内部组合，不新增公共 responsive Sider。打开目录时记住触发者，关闭后返回触发者；选择页面后聚焦新的“进入示例/页面导航”控件，避免焦点留在隐藏抽屉。

代码使用专属横向滚动，页面正文不产生整页横向滚动；长路径允许换行。360 宽仍应能到达目录、主题、标题、样例及能力范围。HTML 原型在聊天 728 宽采用中布局，内层正文高 720、目录同高可滚动；最外层、html/body 无背景/边框/外卡，不用 vh 或 100% 高度。

### 3. Token 与主题映射

| 设计角色 / HTML 变量 | 生产来源 |
| --- | --- |
| `--foreground` / 正文 | Theme alias `color_text` |
| `--muted` / 辅助文案 | `color_text_secondary`，禁用态使用对应 disabled token |
| `--card` / 文档与示例底 | Theme 的 container/background 语义；shell 不强制浅色 |
| `--border` / 分隔线 | `color_border_secondary`；交互边框取组件 token |
| `--primary` / 当前项、链接、主操作 | map `color_primary` 与组件交互派生色 |
| `--secondary` / 代码底、悬停底 | Theme fill/background 语义 |
| `--success` / implemented | Theme success 色；文字仍标识声明范围 |
| `--warning` / partial | Theme warning 色，保留文字说明 |
| `--radius` / 示例框与控件 | seed `border_radius`（Default 6）及具体 Component Token |
| `--font-sans` / 正文 | `SystemFontFamily::ui_sans`，代码为 `ui_monospace` |
| 控件高/内边距 | `control_height`（Default 32）、4-unit spacing scale、现有 Component Token |

HTML 的 `:root` 提供独立文件 fallback；CSS 是设计示意，不能作为原生 token 精确解析的验收。生产取 `ThemeConfig.algorithms`，Default/Dark 是颜色选择，Compact 是可叠加密度（Dark 后追加 Compact）。Compact 减少控件高度和示例内留白，正文仍为可读的 14/22；不把整页缩放。当前页、家族分组、组件身份、普通草稿和当前有效 focus 不因主题按钮重建。品牌与 motion 延续现有功能，UI 收入“主题设置”。所有稳定组件视觉走其 Theme/Component Token，`LayoutStyle` 仅负责外部几何。

### 4. 页面、家族与示例的唯一归属

拟新增以下 Gallery 内部类型/文件，名称为实施建议，非现有公开 API：

- `GalleryPageId`：overview/introduction/tokens/component(identity)。`GalleryPageRoute` 另含 family 与可选 section/sample identity；不使用 URL/HTTP router。
- `GalleryPageDescriptor`：catalog 引用、标题/用途、家族、API/Token 文档元数据和样例 ID 列表。73 个组件 descriptor 从 catalog 创建，没有 factory 的条目自动取得说明页。
- `GallerySampleDescriptor`：stable sample ID、唯一 owner page/family、标题/说明、原有 test ID 清单、mount factory、代码片段/公开头文件映射与状态策略。
- `GalleryPageStateStore`：仅存已访问页的 typed 草稿；与组件宿主/renderer 无引用关系。
- `GalleryPageController`：在事件安全点协调请求、退役、mount、anchor restore 与失败页。

Input 下五组明确为 Input、Search、Password、TextArea、OTP；没有五份 catalog 支持分数。Bidi 示例按其编辑器类型归到对应组，家族共用基础说明。InputNumber、AutoComplete、TreeSelect 有自己的 canonical identity，不并入 Input。Typography 的 Text/Title/Paragraph/Link 留在 Typography；DatePicker 的 range 属于 DatePicker；Space.Compact 属于 Space；Button variants/slots 为同页样例，不另计组件。

既有大函数拆分的落点：

| 当前构造入口 | 页面归属/拆分方式 |
| --- | --- |
| `add_basic_live_samples`、`add_button_and_divider_variant_samples` | 拆分 Button、Input/Search/Password、Divider；保留原 test ID |
| `component_entry` 内 Typography/Divider 实例、`add_typography_layout_slider_samples` | 真实样例移到 Typography/Divider/Flex/Space/Slider/Tooltip 自己页；overview 只保留信息 |
| `add_input_feature_samples`、`add_text_area_samples`、`add_otp_samples`、`add_bidi_samples` | Input 的五组，Bidi 以关联样例归属；不重复构造全家族 |
| `add_common_component_samples` | Tag、Badge、Drawer、Popconfirm、Alert、Spin、Progress 分页 |
| `add_data_and_selection_samples` | Table、Pagination、Menu、Dropdown、InputNumber；共享表格数据为轻量 fixture，流程样例指定唯一主归属，其他页提供关联入口 |
| `add_panel_and_selection_samples`、`add_empty_and_result_samples` | Collapse/Segmented、Empty/Result 分页 |
| `add_notice_samples` | Message/Notification 分开入口，现有共同流程归 Message，Notification 的关联链接可到该流程，不重复计算覆盖 |
| 其余 `add_*_samples` | 按对应 Checkbox/Radio/Switch/Icon/Flex/Space/Select/Card/Tabs/Modal/Form/DatePicker/Breadcrumb/Steps/TimePicker/Descriptions/Skeleton/Popover/Timeline/Avatar/Statistic/Rate/AutoComplete/Tree/TreeSelect 登记 |
| Theme 与 token 展示 | 全局 Token 页；ConfigProvider 页明确 native Theme 映射范围并链接过去 |

“该页依赖其他组件”是允许的：TreeSelect 页的交付备注确实用 Input，Form 用例可以在 TreeSelect 页出现；禁止的是为了访问 TreeSelect 而构造无关的其他页面。迁移时为当前 458 个 stable test ID 建立唯一 owner 或“shell/文档”归属；合并说明文案需说明 replacement，不以删除断言解决迁移。

### 5. 真正的局部结构边界

shell 与 page host 各有稳定 root。初始只构造 shell、轻量 catalog 和默认 overview；直接指定 page 的自动入口只构造目标页。页面/family 导航属于显式结构变化，可运行该页 factory；搜索、页内滚动、当前页普通属性、Theme 与 resize 属于属性/布局变化，不重跑无关 closure。

使用现有 `WindowComponentServices::append_slot` 与 `destroy`，由 Gallery adapter 在正确的 `ActiveReferenceSurfaceHost` 上下文执行追加；删除必须经过现有宿主路径并清理 ReferenceSurface mounted 索引，不直接 `NodeStore::destroy` 绕过 participants。ReferenceSurface 的现有 public-to-example 接口只增加内部 page append/prune 能力。不能把 `runtime::ComponentHost` 的底层 append 当作已经覆盖所有组件 host 的 mount 上下文。

非活动页没有 mounted node、editor、interaction、scene fragment、tick/deadline；共享字体 atlas 与 renderer staging 的可复用容量允许保留。当前页内仍使用现有 clip/离屏 glyph 策略，不声称零离屏 CPU 工作。较大的 Input 先以五个分组减少活动范围；本次不发明通用滚动虚拟化。目录本身最多 73 项且需要搜索和导航，可以常驻轻量导航控件，明确它不属于“非活动页样例”。

备选 retained 全页再 `visible=false` 仍保留成本；备选多窗口会破坏同窗浏览；备选实现通用 VDOM/路由器扩大公共架构，均不采用。只针对 Gallery 引入小型内部页面宿主。

### 6. 草稿、页面历史与错误恢复

状态分为 shell（查询、筛选、类别展开、目录 offset、主题）、page（已提交编辑值、选择、受控 Form 字段、分组、示例折叠、scroll anchor）与 transient（popup、focus、capture、composition、timer）。page 数据按稳定 ID 保存在内存；不保存 node/component/ref handle，不持久化磁盘。

默认保留普通 Input/Search/TextArea 文本、TreeSelect 选择与备注、表单字段与必要的展开键；Password/OTP 离开分组或页面清空，示例旁可见提示。uncontrolled 示例重新挂载时用保存的已提交值作为初始值，不能为了存草稿偷偷改掉示例原本展示的 controlled/uncontrolled 语义。撤销栈、选区和 IME session 不承诺跨卸载保存；仍在同页的 theme/resize 则保持 editor identity 与历史。

导航历史只在内存记录 page/family，后退/前进按钮可用才激活；普通 TOC 不压入页面历史。第一次进入页滚至顶部，返回恢复 `(section/sample identity, local distance)`；重排或内容缩短时夹紧。显式跳转 sample 优先于历史位置。重新选择当前页不重挂载、不抹掉滚动。

挂载失败时清理半挂载 generation、保留 shell 与已保存草稿，显示“此页暂时无法显示 / 重试 / 返回总览”。不要复活旧交互树。只报告本次失败原因；不把 renderer failed/deferred 当作允许忙循环重建页面。

### 7. 切页与输入会话的安全序列

导航回调仅登记 request ticket，在当前单个事件分发完整返回后的安全点执行；不能在旧控件 callback 栈内销毁自己，也不能继续把同批迟到编辑事件当成新页输入。

1. 合并安全点前的导航请求，仅处理最新有效目标，冻结旧页面回调 ticket；保存已提交草稿与锚点。
2. 正常样例临时关闭状态不写入草稿。取消 scrollbar 拖动、pointer capture 与 keyboard press，清理旧 focus；TextInputSessionHost 按 owner/generation 结束旧 session，未提交 preedit 取消，不隐式提交表单。
3. 使用宿主 destroy 退役旧 page root、scope、popups、输入 boundary、caret 和动画/deferred 回调；所有 window-layer 后代仍按 owner 归属于旧页。TreeSelect 的“拒绝关闭”仅演示组件受控行为，不能拒绝页面销毁。
4. 确认旧 mounted 索引已清理后 append 新页，分配新 page generation，建立具名 root/scrollbar/TOC/sample handle。生命周期退役不直接触碰 SDL 或 GPU handle。
5. 布局完成后只刷新新页锚点，恢复/夹紧 scroll，更新 clip/HitTest；旧 anchor/request/IME/callback generation 一律失效。按导航来源保留可见 sidebar focus，窄面板切页聚焦正文的显式导航控件，不自动聚焦编辑器。

原生输入事件由现有 session/token 机制判定有效性；不要仅用 page ID 丢掉新窗口必要事件，也不要清空整个宿主输入队列。输入合同测试需覆盖同批后续 TextCommitted、CompositionChanged、pointer up 和延迟回调。Modal/Drawer 用户输入边界继续阻止背景导航；测试可直接调用页面控制器验证 owner retirement，但不能让 Ctrl+K 穿透模态。

页内 Escape 优先交给最上层 popup，未消费后关闭窄目录，再清搜索；Tab/Shift+Tab 保持普通控件语义。Ctrl+K 不在 composition 或模态期间抢焦点。正文 PageUp/PageDown/Home/End 仅在正文滚动入口聚焦且没有编辑器消费时处理。

### 8. 只更新活动页的滚动与呈现

`GalleryDocumentViewport` 从固定六 section 表演进为当前页的 typed section/sample anchor 集；每次 page generation 改变清理缓存。shell 的 navigation 和 document 两条 `GalleryScrollRange` 保留，中窄布局仍共享 `gallery_layout_metrics` 的区域定义。窄目录成为独立面板，不把目录高度加进正文 extent。

`GallerySubmitter` 不再扫描全部 catalog surface 或依赖“最后五个 mounted surface”；注册明确的 shell/page/scrollbar handle，页面增量 mount 后检查 generation。anchor 只在 layout/结构变化后刷新；纯 scroll 走 translation，不重新构造/measure 页面。滚轮继续走 `GalleryEvents::dispatch -> inputs -> 未消费时区域 scroll`，性能自动入口覆盖这条路径，不能只直接调用 `scroll_by()`。

所有视觉更新继续写 logical CPU scene v3，通过共同 SceneResources 事务与 SceneBackend；保持 packed ABI v2、SceneDeviceMetrics、owner/epoch、失败恢复及析构顺序，不新增 shader/私有上传。dirty 通过 DirtyQueues/FrameRequestState 唤醒，动画本帧 invalidation 和明确的下一帧请求仍分开；page disposal 之后不得有旧 timer 造成持续帧。

### 9. 测试迁移与验收边界

- model/catalog：73 个 identity 唯一解析、7 类数量、六状态、空结果、中文/英文/家族搜索、没有 atomic capability 分母时没有实现率。生成器数据一致性检查维持不变。
- page fixture：替换 `navigation_control_count + N`、固定 307 live samples、全局 Text 前缀搜锚点；使用 page/sample/test identity 获取句柄。原有原生功能断言迁到相应页，overview 不再要求全页组件数量。
- viewport：覆盖 1440/960/728/480/360、双向 resize、page generation、家族切换、TOC、目录/正文隔离、嵌套 wheel、scrollbar cancel 与 focus reveal。
- lifecycle：Button/Input/TreeSelect/Upload 往返、普通草稿/敏感清空、拒绝关闭弹层、capture、preedit、迟到事件、回调重入、挂载失败、资源退役、idle。跨页卸载允许重建，普通同页属性更新仍要求 identity 不变。
- 性能：同一 preset/build configuration 记录首次 mount、切页、真实 wheel 分发、scene sync 的分项耗时以及节点/editor/hit/上传计数；加入未访问登记不得增加当前页样例工作；多轮往返无活资源增长。报告测量条件，不预设收益百分比/FPS，也不拿旧的调试器样本冒充当前基线。

构建统一使用 CMakePresets/Ninja Multi-Config。平台通用计划选择 `windows-msvc-headless` Debug/Release 一次，构建相关 Gallery definition/model/viewport/frame targets 和定向 CTest；这也保持当前实际 HEADLESS/MSVC 边界验证。Linux 原生阶段不再要求重复同一通用 suite。若实施地点改用 Linux 完成平台通用逻辑，记录实际 `linux-gcc` preset，Windows 只补 HEADLESS/toolchain 受影响集成，不再次要求整套通用逻辑。

Windows 原生用 `windows-msvc` Release/MSVC/实际 D3D12 跑新建的 `--page-acceptance`（拟新增自动入口），覆盖切页后 shader/clip、字体、host input area/cursor 与窗口 resize；不是系统 IME 候选窗或人工桌面体验验证。Linux 原生用 `linux-gcc` Release，在 `tools/run_background_wayland.py --log-dir out/... -- <binary> --page-acceptance` 中运行。现有脚本依赖 `dbus-run-session`、Mutter headless、Wayland 与 Vulkan，虚拟 monitor 为 1920×1200，只等 socket 约 10 秒、测试默认 30 秒；不保证本机有可用 GPU/driver/字体/输入服务。应显式设置有限 timeout，记录缺依赖、连接/renderer 失败或超时，绝不回退用户当前 DISPLAY/WAYLAND_DISPLAY。本轮没有运行它，不声称后台环境可用。

只构建相关目标并运行受影响自动检查，不要求每次修改全量 CTest 或原生矩阵。Windows/Linux checkbox 独立；失败或缺平台证据保持未完成，不回退另一平台。结果只留精简 Markdown；日志/计时样本/图片只在 out。tasks 没有人工检查或用户确认项目。

### 10. 原型交付与边界

`out/gallery-page-design/20261103/gallery-component-pages.html` 是单文件、内嵌 CSS/JS、无远程依赖。它从当前 manifest/overlay 提取真实 73 条 metadata，提供所有条目的独立内容视图；Button、Input（五组）、TreeSelect 交互为 HTML 演示，Upload 是未实现说明页。其他已有组件页面明确显示“此设计稿未绘制该组件交互示例”，而不是声明没有原生实现。

原型为了直接展示组件页，初始打开 Button；生产正常启动仍进入组件总览。响应式原型按容器宽度重排，适配聊天内 728 宽和独立文件。使用仓库已捆绑的 Ant Design icons 并内嵌 MIT 许可，不请求远程图标。

原型支持目录/状态筛选、页内 TOC、主题/密度、代码展开、范围说明、普通草稿恢复及 TreeSelect 单选 popup。所有设计模拟有持续可见的小标记。原始 scope/evidence 保留在来源区，未运行任何原生验收。父线程负责聊天内展示；本轮不启 server、不启动浏览器/原生窗口、不截图。

设计稿检查：聊天内 HTML 渲染器在 1440/728/390 宽度加载无控制台错误；原型内核对 73 条路由、Input 草稿往返、TreeSelect 弹层、Upload 无伪造上传器、空搜索及 Dark/Compact 切换通过。73 条状态与原始 scope 数据逐项匹配 overlay。以上仅验证设计原型，未编译 C++ 片段或运行原生 GUI 验收。

## Risks / Trade-offs

- [卸载丢失 uncontrolled 草稿或测试语义] → 在样例 descriptor 显式声明草稿策略，以初始值恢复；同页更新不改 controlled 模式，跨页撤销栈边界如实说明。
- [popup 在 window layer 中脱离正文 clip，但 owner 仍存活] → 按组件祖先/owner 销毁，定向验证 boundary、capture、session、动画与 scene fragment 全部退役。
- [ReferenceSurface 不是普通 participant，追加上下文或清理漏掉] → 新 adapter 统一 ActiveReferenceSurfaceHost 与 services.append_slot，使用现有 destroy 后 prune，失败路径单测。
- [拆混合大函数时丢失 Form/Table/Notice 流程] → 固定主归属与关联页，逐个迁移旧 test ID 和功能断言，避免只保留好看的简单示例。
- [metadata 含历史 Linux pending 或 tasks 指针] → 原样呈现来源，不推断当前实测；状态修改留给有对应证据的独立变更。
- [用户误把 11 implemented/73 条目理解为完成度] → 只称目录状态分布；没有进度条/百分比，不把 partial 当零或一。
- [主题或 resize 导致重建] → 属性更新与结构导航分开，测试 identity、mount counter、anchor clamp；Compact 不整体缩放。
- [首访页面挂载可能短时较贵] → 仅当前页/活动家族挂载，记录实际成本；不通过后台预挂全部页抵消首访成本。

## Migration Plan

先落实纯页面模型与 catalog/schema 合同，再实现内部页面宿主和生命周期夹具，然后分批迁移全部样例及测试，最后接入 shell/响应式导航和定向平台自动入口。每阶段含实现与独立自动校验；必要的临时旧/新 fixture 只用于迁移测试，最终正常启动不留下两种产品模式。

回退采用 revert 对应已验证阶段的代码提交，catalog/上游锁无格式迁移、草稿仅内存，无持久化数据风险。本轮审阅后仅提交规划文档，HTML 设计稿保留在忽略目录 `out/` 并在聊天中展示；没有 apply 授权前不执行实施。

没有需要阻塞本轮规划的实质问题。尚需实施时取得的证据是页面宿主实际生命周期测试和各平台自动运行结果；这些已在 tasks 中作为自动工作追踪，不是本轮设计已验证的事实。

### 11. Windows 组件页反馈修复

`69fdcdc8` 的实际 Windows 页面反馈要求四项修复：目录左对齐；页首及正文为外扩焦点装饰保留可见空间；来源折叠正文逐段排布；Space.Compact 滚动往返后不出现中部横线。

目录使用内容 slot 内的左对齐布局并保持整行点击区域。正文的硬裁剪采用真实 document viewport，内容宽度和内侧留白另行放置；滚动 extent 与内容可用高度一致。来源显式组合为纵向内容，长路径按可用宽度测量。它们不改变稳定组件的全局默认排版或 Theme schema。

横线按最终绘制域、物理 clip 和 source-over 复现；若确认共用 renderer 边界错误，在原共同路径修复，不增加组件私有上传或用额外色条补画。继续保留 056 的一物理像素 AA、完整 draw guard、真实 ancestor hard clip 与无 feather 的分区所有权，以及 logical scene v3、packed ABI v2 和 48/96/112-byte 实例布局。

本轮按新增第 9 节的局部自动回归跟踪；此前 Windows 全集与 Linux 自动平台项目仍各自依据原定范围及实际结果记录，不从四项修复推导整个平台验收通过。

### 12. 新组件局部视觉巡检

2026-11-06 用户要求对 106/107 已新增或扩充的组件补自动视觉验收。采用隐藏原生窗口与现有 renderer 离屏 GPU 回读，不显示桌面窗口、不抢系统焦点。有限入口按真实 sample root 分段滚动，捕获 Default/Dark/Compact 和窄宽度局部图像；输出只在 out。巡检覆盖以实际登记样例和状态为准，功能断言或单点颜色检查不替代整段视觉结论。

图像需检查空白、正文/控件裁剪、重叠、对齐及滚动前后稳定性。发现问题先保存失败图，再修共享布局或组件实现并补针对性回归；保留未完成组件与未捕获交互状态的边界，不增加用户人工任务或要求确认。
