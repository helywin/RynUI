# RynUI 既有实现审查与分阶段调优

日期：2026-10-29。目标：先消除既有组件的性能浪费、错位、裁剪和图标问题，再补齐与 Ant Design 不一致的动效及原生能力。本文是后续各阶段的执行入口；具体实现仍拆成独立 OpenSpec change，不用一次大改同时重写组件、布局与 renderer。

**结论：当前实现不能认定为完全符合 Ant Design。** 保留式组件、受控交互、主题和生命周期已有较多合同与测试，但部分默认布局公式和图标选型存在可定位偏差，许多动画尚未实现，真实性能与原生视觉也不能由 headless 测试代替。已声明的首版限制仍需进入补齐计划，不能当作永久豁免。

## 1. 基线与排除范围

- RynUI 审查基线：`ac0d1196236b1132b74cf5cb6e5d96b1f9ff9b64`，父提交 `7ca31dc26fa59a924ed7163043609c66f13c26c1`。这是 `099` 正式图标替换实现后的版本。
- Ant Design 对照：`6.6.5` / `4a39f54842eade4e565ab336ef6097cd7e723cdd`。本地上游 checkout 的 HEAD 与此一致，工作树干净；生成资产的 baseline checker 本轮通过。Context7 文档用于核对主题/配置语义，具体样式结论以此固定版本源码为准，不静默升级参考版本。
- 按用户要求排除正在进行的 [100 滚动同步优化](../../openspec/changes/100-20261029-reduce-gallery-scroll-synchronization/proposal.md)：Input 不可见字形同步、Gallery 目录锚点缓存、滚动采样计数，以及该线程正在改动的对应测试。本轮不接管、不重复修复、不评价该变更的最终效果。
- 审查期间另一线程已推进到 `47b0d7bf` 并继续修改 Input/Gallery；本文 findings 引用冻结基线，相关被审查源码当时相对基线无差异。后续 apply 前必须再次确认目标调用链仍存在。
- 本轮完成目录与公开范围核对、重点调用链审查及文档校验；没有新构建、benchmark、窗口操作、GPU 截图或动画录像。下文的性能复杂度是源码结论，绝对耗时尚待测量；几何公式差异不是本轮人工截图验收。

原生对齐目标包括外观层级、Token 配方、布局、图标、状态、交互与动画。React/DOM/CSS 字段不照抄；桌面适配差异必须有逐项说明。已通过的历史测试仍然有效于其记录的源码和环境，不因为本轮审查而回退，也不自动升级为本基线的完整验收。

## 2. 全量覆盖边界

依据 [support-overlay](../../gallery/ant-design/6.6.5/support-overlay.json) 核对全部 73 个参考项：`implemented` 11、`partial` 39、`planned` 22、`deprecated` 1。前两类共 50 个已有实现/原生映射条目，是本轮调优覆盖对象；这不是 50 个组件均已逐行重审或通过视觉验收的声明。

下表 `C` 表示核对目录、公开范围与文档，`S` 表示另审查了与 findings 相关的局部源码调用链。所有条目的当前 GPU 视觉/动画对照仍待执行；`implemented` 也只代表已登记的原生功能范围。

| 已有条目 | 目录状态 / 本轮深度 | 调优和补齐重点 |
| --- | --- | --- |
| [Button](../button.md) | implemented / S | 状态曲线、loading、wave、图标基线、快速反向；保留已有动效机制 |
| [Icon](../icon.md) | implemented / S | 禁止动作字符占位；核对图标名称、尺寸、双色、DPI、spin 生命周期 |
| [Typography](../typography.md) | partial / C | 五级标题、文本基线、截断/编辑提示、多行与 bidi；富文本能力单独补齐 |
| [Divider](../divider.md) | implemented / C | 标题与横线对齐、点划线、窄宽、方向和主题间距 |
| [Flex](../flex-space.md) | implemented / C | 真实文本基线、wrap/reverse、伸缩、零面积恢复、方向 |
| [Space / Compact / Addon](../flex-space.md) | implemented / C | 相邻边框、焦点轮廓、尺寸继承、首末圆角和混合控件 |
| [Checkbox](../checkbox.md) | implemented / S | 勾选/半选动画与颜色过渡；已有 wave 不等于选中动画 |
| [Radio / RadioButton / Group](../radio.md) | implemented / S | 圆点动画、组尺寸和连接边、动态顺序与受控拒绝 |
| [Switch](../switch.md) | implemented / S | handle 与 track 颜色同步、按压、loading、文字和图标布局 |
| [Slider / Range / Multi](../slider.md) | implemented / C | thumb/rail/marks/Tooltip 对齐，交叉端点、方向、拖动性能 |
| [Input / Password / Search / TextArea / OTP](../input.md) | implemented / C | affix/clear/眼睛/搜索/分格图标、caret/bidi/IME、Compact；排除 100 当前同步工作 |
| [Tooltip](../tooltip.md) | implemented / S | 十二方位和箭头、进入退出动画；hover delay 不能代替动画 |
| [AutoComplete](../auto-complete.md) | partial / C | 建议弹层与富内容；分组/异步/虚拟化/RTL/动画缺口 |
| [Select](../select.md) | partial / S | 弹层行高与选中图标；multiple/tags/分组/富选项/虚拟化缺口 |
| [TreeSelect](../tree-select.md) | partial / C | Input/Popover/Tree 组合尺寸；多选/搜索/动态拓扑/虚拟化/动画 |
| [DatePicker](../date-picker.md) | partial / S | 月历网格公式、短窗口全部日期可达；Range/格式/locale/动画 |
| [TimePicker](../time-picker.md) | partial / S | 列滚动与 active 显露、短窗口 footer；Range/12小时/locale/动画 |
| [InputNumber](../input-number.md) | partial / C | 步进按钮/图标、数值基线；stringMode、formatter/parser、长按重复 |
| [Form / FormItem](../form.md) | partial / C | label/错误/帮助文字对齐；横向/inline、动态字段、依赖/异步校验、错误显露 |
| [Rate](../rate.md) | partial / C | 半星边界、hover/选择中间态、富提示、字体与 DPI；不宣称已做逐帧验收 |
| [Breadcrumb](../breadcrumb.md) | partial / C | separator 是图标还是文本语义、基线和窄宽；动态项/换行/RTL/菜单 |
| [Dropdown](../menu-dropdown.md) | partial / C | 命令图标、窗口边缘定位、进退场；hover/split button/自定义内容 |
| [Menu](../menu-dropdown.md) | partial / S | icon-label 间距 Token、展开图标/动画；横向溢出和 flyout 模式 |
| [Pagination](../pagination.md) | partial / C | 正式翻页/跳段图标、按钮对齐；size changer、quick jumper、simple |
| [Steps](../steps.md) | partial / C | 图标/编号/连接线和文本基线；动态结构、响应式与展示变体 |
| [Tabs](../tabs.md) | partial / S | 极窄导航、indicator 动画；vertical/RTL、富标签、overflow popup、editable |
| [Avatar / AvatarGroup](../avatar.md) | partial / S | 字体 auto-fit、圆形安全区、溢出；图片管线、断点尺寸、富 Group slots |
| [Badge](../tag-and-badge.md) | partial / C | 偏移、跨位计数、processing 和滚动数字；Ribbon 等能力 |
| [Card](../card.md) | partial / C | header/extra/actions、loading、阴影；Grid/Meta/Tabs/图片及圆角裁剪 |
| [Collapse](../collapse.md) | partial / S | 展开高度、箭头与裁剪动画；ghost/borderless、动态项和方向 |
| [Descriptions](../descriptions.md) | partial / S | 标签/内容分区、span/列宽/焦点；动态项、自动断点与 RTL |
| [Empty](../empty-result.md) | partial / C | 正式原生图示与上游插画差异；图片资源能力，不能用状态字符代替 |
| [Popover](../popover.md) | partial / S | 进退场、箭头原点、内容边缘；contextMenu、内置滚动和 RTL |
| [Segmented](../segmented.md) | partial / S | 选中 pill 位移/宽度动画；动态项、方向和不等宽标签 |
| [Statistic](../statistic.md) | partial / C | 局部文字更新成本、数字基线；Timer/locale/值动画 |
| [Table](../table.md) | partial / S | 局部更新避免重排、双 caret 表头、长标题；过滤/固定列/展开/虚拟化 |
| [Tag / CheckableTag](../tag-and-badge.md) | partial / C | close 图标、关闭收缩、进退场、焦点与受控拒绝 |
| [Timeline](../timeline.md) | partial / C | dot/连接线/标题位置与 loading；动态项、横向、RTL/动画 |
| [Tree](../tree.md) | partial / S | hover/focus 局部更新、switcher/checkbox；动态项、多选、strict/异步/虚拟化 |
| [Alert](../feedback.md) | partial / C | 状态/close 图标、退出与布局收缩；banner/marquee |
| [Drawer](../drawer.md) | partial / S | 四方向进退场和阴影；loading、extra、push/destroy/RTL |
| [Message](../message-notification.md) | partial / S | Loading spin、hover 计时、进入退出、同 key 更新与堆叠 |
| [Modal](../modal.md) | partial / S | 遮罩与面板时序、关闭后焦点；destroy/lazy、响应式宽度等 |
| [Notification](../message-notification.md) | partial / S | Loading、hover 暂停、四角进退场、富编辑内容和堆叠 |
| [Popconfirm](../popconfirm.md) | partial / S | status/close 图标、气泡动画和异步反馈；触发方式扩展 |
| [Progress](../feedback.md) | partial / S | 半透明弧重叠、端点与图元成本；active/steps/gradient/success 段 |
| [Result](../empty-result.md) | partial / C | 状态 Icon 与 403/404/500 插画差异、标题/actions 对齐 |
| [Skeleton](../skeleton.md) | partial / C | 静态占位尺寸；active/shimmer、Image/Node 与停帧合同 |
| [Spin](../feedback.md) | partial / S | 复用已有 spin 生命周期；description、遮罩、fullscreen/delay/percent |
| Theme / ConfigProvider 原生映射 | partial / S | 全局尺寸/禁用/locale、局部方向与全局覆盖、Token 完整性和优先级 |

其余参考项是 FloatButton、Grid、Layout、Masonry、Splitter、Anchor、Cascader、ColorPicker、Mentions、Transfer、Upload、Calendar、Carousel、Image、Listy、QRCode、Tour、Watermark、Affix、App、BorderBeam、Util，以及 deprecated List。它们没有对应稳定实现，本轮不把“调优已有代码”扩大成全部新组件开发，也不能计入实现完成率。

基础设施另覆盖：Reactive 订阅、layout cache、glyph CPU store/GPU buffer、atlas、rounded-effect 上传规划、场景 dirty 与 benchmark 范围。已经存在 intrinsic cache、glyph atlas 索引、dirty 分层、稀疏上传与失败重试，不能笼统写成“没有缓存/每帧全量上传”。

## 3. 性能整改清单

以下均为 P2 级可扩展性风险；没有本轮 benchmark 支持绝对耗时排序。前四项有明确局部修改引发全局工作的路径，优先做最小重现与修复；后两项先测量再决定改造范围。不得引用 100 的滚动结果作为这些修改的收益。

### TUNE-PERF-001 等长文字替换复制整个 glyph store

- 证据：[glyph_scene.cpp](../../src/graphics/glyph_scene.cpp) L211–265，尤其 L248–253；[text_scene_service.cpp](../../src/text/text_scene_service.cpp) L670–699。旧 range 非空时先创建完整 replacement，之后才判断新旧是否等长。
- 触发：大页面中将一个 Statistic/Text 的 `1000` 改为 `1001`。局部内容更新也有 O(G) 全局 glyph 复制/分配，同帧 K 处更新可累积为 O(KG)。与 100 的不可见 Input 裁剪无关。
- 整改：先加 alias-safe 等长原位更新路径，保持 dirty range、邻居内容和异常合同；变长存储方案留给测量结果决定。
- 验收：背景 1k/10k/100k glyph，固定更新 4 字符；记录分配、复制量、更新范围。等长更新不得再分配全 store，邻居 range/内容保持。现有 Input 选区 allocation benchmark 不覆盖此内容更新场景。

### TUNE-PERF-002 Glyph GPU buffer 精确长度扩容

- 证据：[glyph_gpu_resources.cpp](../../src/renderer/common/glyph_gpu_resources.cpp) L147–175。容量仅设为当前 live count，超过峰值就创建新 buffer 并全量上传。
- 触发：逐字输入产生新可见 glyph，或逐条追加日志；每次越过历史峰值一个 glyph 都可能重新创建 buffer。
- 整改：使用受能力上限约束的几何扩容，分开 live count 与 capacity；同步 preflight，保持 owner/epoch、失败重试和销毁合同。
- 验收：稳定背景逐次追加 1,000 glyph，记录 reallocation 次数、upload bytes 和 p95；容量增长次数应呈对数级，容量内删除/恢复不重建。补最大能力边界及上传失败恢复。

### TUNE-PERF-003 Tree hover 重新投影整棵树

- 证据：[tree_component.cpp](../../src/component/tree_component.cpp) L933–936、L238–300、L342–346；[tree_model.cpp](../../src/component/tree_model.cpp) L24–39、L82–119。hover 调用通用 refresh，多组线性 membership 查询嵌在逐行循环中，并无条件标记 scene structure dirty。
- 触发：展开/勾选较多节点的 Tree 在行间移动鼠标或键盘焦点；接近 1,024 节点上限时最坏有 O(N²) 状态查询。
- 整改：分离纯 hover/focus 材质更新和结构/选择投影；固定拓扑使用索引或 bitmap；仅实际拓扑变化触发 structure dirty。
- 验收：128/512/1,024 节点，记录访问量、分配与 composer rebuild；hover 只更新旧/新行相关状态，同时保持禁用隔离、级联勾选、富子控件和 Tab 顺序。

### TUNE-PERF-004 Table 选择变化重新复制并排序全部记录

- 证据：[table_component.cpp](../../src/component/table_component.cpp) L857–867、L482–489、L143–170、L512–530。selectedRowKeys 更新进入 refresh，重建投影并在排序开启时 stable_sort；visible/selected membership 逐行线性查找。
- 触发：已排序 Table 勾选一行、改变 bordered 或分页后更新选择；排序输入未变也重新调用 comparator，无分页 membership 最坏 O(N²)。
- 整改：按数据/排序/分页 revision 缓存投影，选择/材质变化不重新排序；用索引集合减少 membership 扫描。此修复不以虚拟列表为前提。
- 验收：1k/5k/10k 行，comparator 计数、更新/选择/翻页 p95、分配和访问行数；单改选择/bordered 的排序比较次数为零，row key、cell 草稿和焦点身份不变。

### TUNE-PERF-005 Reactive 高扇出订阅扫描

- 证据：[reactive.cpp](../../src/reactive/reactive.cpp) L54–73、L98–105、L139–144。observer 每次执行拆除并重建依赖，subscribe/unsubscribe 都线性扫描 subscribers；同一 Signal 的 K 个 observer 更新最坏可有 O(K²) 扫描。
- 整改前先测量：K=1/64/256/1024/4096，分别测稳定依赖与动态依赖切换；现有单条 Signal→Memo→Effect allocation 基准不能证明高扇出扩展性。
- 若确认为热点：保留未变依赖或使用稳定订阅句柄，避免整体重写 reactive。验收必须保留异常、销毁、动态依赖和重入调度合同。

### TUNE-PERF-006 Rounded effect 每插入一个 dirty range 都排序

- 证据：[rounded_effect_gpu_resources.cpp](../../src/renderer/common/rounded_effect_gpu_resources.cpp) L70–75、L154–176。每次 push 后 sort/merge；R 个离散区间的规划成本最坏接近 O(R² log R)。
- 整改：先收集两组区间再统一 sort/merge，或对有序集合线性归并；保持上传失败后 dirty 和计数可重试。
- 验收：1k/10k effects 中修改离散 256/1k ranges，比较 planning CPU、最终上传区间/字节与失败恢复。现有 idle/等值更新基准不能覆盖这一场景。

## 4. 布局、图标与绘制整改

### TUNE-VIS-001 Menu 图标间距硬编码

- [menu_component.cpp](../../src/component/menu_component.cpp) L417、435、442 固定使用 8；[menu_theme.hpp](../../include/ryn/menu_theme.hpp) L29–40、61–83 没有对应间距 Token。
- 上游 [Menu token 配方](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/menu/style/index.ts#L947) 为 `controlHeightSM-fontSize`，默认 24−14=10。默认 icon-label 间距少 2 logical px，字体与 Compact 覆盖也不能按上游配方变化。
- 整改：补 typed Token，测量/放置共用；同时纠正 catalog 中该原生可映射间距的 web-only/catalog-only 分类。验收三主题、字体覆盖、嵌套菜单、窄宽及图标/文本真实 bounds。

### TUNE-VIS-002 DatePicker 月历配方偏差

- [date_picker_theme.cpp](../../src/theme/date_picker_theme.cpp) L21–25 默认 panel width=288、统一 padding=12、row height=40。
- 固定上游 [token.ts](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/date-picker/style/token.ts#L133) 与 [panel.ts](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/date-picker/style/panel.ts#L358) 为同宽 288、body 横 padding=18、纵 padding=8、row height=36。网格起点与逐行高度配方不同；这是源码确认的几何偏差，尚无本轮 GPU 对照。
- 整改：拆开 header/body/footer 度量并使用同版配方，不能仅缩总高度。验收同月六周、工作日行、hover/选中/禁用、三主题和各 scale 的 cell bounds/截图。

### TUNE-VIS-003 Table 正式图标选型仍不匹配

- [table_component.cpp](../../src/component/table_component.cpp) L331–346 将 Small Text Button 紧随标题，L547–550 使用 ColumnHeight/ArrowUp/ArrowDown。
- 上游 [useSorter.tsx](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/table/hooks/useSorter.tsx#L146) 同时保留上下 Caret，当前方向着色；[sorter.ts](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/table/style/sorter.ts#L50) 让标题伸缩、sorter 位于另一端。
- `099` 已解决这一路径的字符占位，但不等于选型/排列完全对齐。整改为保留双 caret、匹配颜色和 header 布局；验收无序/升序/降序、受控拒绝、长标题/窄列、三尺寸，以及 header 子控件的点击和焦点归属。

### TUNE-VIS-004 DatePicker 短窗口日期不可见

- [date_picker_component.cpp](../../src/component/date_picker_component.cpp) L870–902 缩小 popup，却仍按完整 header+7 行放置；L749–778 的键盘 active 更新没有滚动显露目标。[文档](../date-picker.md) 已公开该限制，属于既有产品限制整改。
- 最小场景：窗口高约 300，触发器 y≈130、高 32；可用弹层高度约 126，完整月历约 344，后几周被裁掉。键盘可以改变不可见 active 日期，但不能看见目标。现有 finite-bounds 测试只证明没有越界，不能证明日期可达。
- 整改：有限 popup 内提供滚动/显露或明确压缩方案，footer 也须可达。验收触发器上/中/下、六周/月末、富 footer、滚轮边界和焦点恢复。TimePicker 有列滚动，仍须独立测小高度下 actions/footer 可达性。

### TUNE-VIS-005 极窄 Tabs 丢失全部标签区

- [tabs_component.cpp](../../src/component/tabs_component.cpp) L656–678：overflow 时两个 arrow 可各占 width/2，剩余 viewport 为零，所有标签不可见。
- 场景：两个以上正常标签，容器宽不超过两个滚动按钮之和；箭头仍显示，但无法滚出可见标签。此为源码推导，尚未实窗复现。
- 整改：定义最小标签区或单一 overflow 入口。验收 0→极窄→正常恢复、Line/Card、选中/焦点可见、滚动及面板身份；不能以“不越界”代替可用性。

### TUNE-VIS-006 Progress 半透明圆弧重叠

- [feedback_component.cpp](../../src/component/feedback_component.cpp) L13–14、L914–962 以 192 个相互覆盖的圆点构成前景弧，每个独立混合；每个 Progress 预留 194 层。底轨已用单次 outline，但前景仍重叠。[文档](../feedback.md) 已声明半透明前景交叠加深。
- 整改：通过共同 renderer 的覆盖/路径能力避免重复 alpha 混合，同时测量图元成本；不得添加组件私有 GPU 路径。固定采样数本身不能证明帧耗时瓶颈。
- 验收：circle/dashboard，0/1/50/99/100%，alpha=0.25/0.5/1、三尺寸和 DPI；检查厚度、端点、缺口及背景混合，记录批量实例 CPU 与上传量。

### TUNE-VIS-007 Drawer 四方向阴影未映射

- [theme_snapshot.cpp](../../src/theme/theme_snapshot.cpp) L1453–1471 从 Modal 复制 effects；[modal_component.cpp](../../src/component/modal_component.cpp) L1199–1202 使用单份阴影。
- 上游 [Drawer style](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/drawer/style/index.ts#L126) 为四方向分别选用 `boxShadowDrawer*`。这是 [Drawer 文档](../drawer.md) 已声明的视觉缺口。
- 整改：按 placement 选择阴影并保留显式覆盖/继承。验收同背景四边、浅/深/品牌主题、mask 开关和 DPI，颜色更新不得重建正文。

## 5. 动画整改清单

### TUNE-MOTION-001 基础选中动画不能由 wave 替代

- [selection_component.cpp](../../src/component/selection_component.cpp) L1737–1748、L1780–1799：Radio 圆点、Checkbox 勾选/半选和相关材质直接切换。Switch handle 已有动画，但 track 颜色在 L1710–1715 直接取目标色，也应核对同步。
- 上游 [Checkbox](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/checkbox/style/index.ts#L96)、[Radio](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/radio/style/index.ts#L282) 各有 scale/opacity、时长/曲线，不能统一套同一缓动。
- 整改：复用 AnimationRuntime，分别接入状态材质和标记呈现通道，快速反向从当前呈现值 retarget；受控请求被拒绝不得提前显示已接受状态。
- 验收：确定时钟起/中/终点、indeterminate、快速反向、受控拒绝、motion=false/reduced、隐藏/销毁和 idle；GPU 核对勾选形变及 DPI。

### TUNE-MOTION-002 浮层缺少进入与退出呈现

- [tooltip_component.cpp](../../src/component/tooltip_component.cpp) L222–231、L812–834 直接显隐/放置；[modal_model.cpp](../../src/component/modal_model.cpp) L26–32、[modal_component.cpp](../../src/component/modal_component.cpp) L294–315 立即同步逻辑可见性、overlay 和事件。Drawer 复用 Modal 模型；Popover/Popconfirm 文档同样声明动画未覆盖。
- 固定上游分别为 Tooltip `zoom-big-fast`、Popover `zoom-big`、Modal `zoom`、Drawer 遮罩 fade 与四方向位移；参见 [zoom](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/style/motion/zoom.ts)、[Drawer motion](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/drawer/style/motion.ts)。
- 整改：区分逻辑开合和过渡呈现，明确输入边界、focus restore、afterClose/afterOpenChange、退出中销毁与重新打开的时机；按实际组件对齐，不让所有浮层套一个动画。
- 验收：12 方位、四边、父子浮层、连续开关/受控拒绝、退出中销毁、motion=false，核对 transform origin、遮罩、层级和焦点。

### TUNE-MOTION-003 Collapse 与 Segmented 直接跳到目标几何

- [collapse_component.cpp](../../src/component/collapse_component.cpp) L455–459、L502–505 直接在 body 高度和零之间切换；[segmented_component.cpp](../../src/component/segmented_component.cpp) L645–669 逐项直接显示 selected 背景，没有独立滑动 pill。
- 整改：Collapse 的高度/clip/箭头协调，Segmented 保留一个可更新位移/宽度的选中背景；对照 [上游 Segmented](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/segmented/style/index.ts#L288)。
- 验收：长正文、富输入、展开中 resize、反向；横/纵、不等宽/block、字体/主题变化；动画不得重新执行内容 slot。

### TUNE-MOTION-004 Notice Loading、hover 计时与进退场

- [notice_component.cpp](../../src/component/notice_component.cpp) L283–287、L641–646 只换 Loading 图标名而不 spin；L745–755 到期直接 close，L583–585 立即 destroy。[文档](../message-notification.md) 明确静态 Loading、无 pauseOnHover 和动画。
- 上游 [Notification](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/notification/useNotification.tsx#L85) 与 [Message](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/message/useMessage.tsx#L96) 默认启用 pauseOnHover。富通知中阅读/操作时仍到期消失，应列为体验补齐。
- 整改：复用已存在的 Icon spin，明确定时剩余量和重入更新；补进入/退出/堆叠移动。Skeleton shimmer、Badge processing/数字滚动、Tag/Alert 退出、Tabs indicator 单列后续子阶段，不能用一项“加动画”整体勾选。
- 验收：同 key loading→success 保留草稿/身份；duration=0、反复 hover、更新续期、失活恢复、关闭回调一次；无 entry 时不持续请求帧。

## 6. 字符占位与缺字专项

这是用户追加的独立必修项。必须区分三种问题：字符假扮图标、正式图标选型错误、文本/字形解码或 fallback 缺失。不能看到问号就默认是同一个原因。

### TUNE-ICON-001 Progress 成功和异常仍使用字符

- **确认残留**：[feedback_component.cpp](../../src/component/feedback_component.cpp) L695–698 使用 `✓` / `!`，L724–725 将它们实际挂为 Text，`099` 没有覆盖这里。状态内容依赖系统文本字形，形态也与上游不一致。
- 对照 [固定版本 Progress](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/progress/progress.tsx#L186)：默认信息展示下，Line 成功/异常分别用 `CheckCircleFilled` / `CloseCircleFilled`；Circle/Dashboard 用 `CheckOutlined` / `CloseOutlined`。上游自定义 format/内部百分比显示另有文字语义，不应一律换图标。
- 整改：保留正常百分比 Text，状态使用正式 Icon，按 type/status 切换可见性并保留身份；异常不是简单换成 Exclamation 图标。
- 验收：三类型 Normal→Success→Exception→Normal、percent=100、showInfo、三尺寸/主题/DPI。禁止仅凭此残留认定用户先前截图的问号来源。

### TUNE-ICON-002 Gallery Loading slot 仍用省略号

- **确认残留**：[token_gallery_definition.cpp](../../examples/token_gallery/token_gallery_definition.cpp) L1684–1691 的 `gallery.button.icon-end` 是实际 live Button，其 `ButtonLoadingIcon` 使用 Text `…`；L1701–1704 的按钮会切换 loading，路径可达。
- 整改：改为正式 `LoadingOutlined`，复用 Icon spin 与 motion policy；保留 loading delay、iconPlacement、focus 和自定义 slot 的生命周期合同。
- 验收：实际切换加载，检查正确来源、位置、旋转、中止和 motion=false；不用删掉自定义 slot 样例来绕过问题。

### TUNE-ICON-003 统一 Gallery 的箭头型分隔符

- [token_gallery_definition.cpp](../../examples/token_gallery/token_gallery_definition.cpp) L3517 自定义 Breadcrumb separator 为 Text `›`。这是有意演示自定义 slot，不是默认 Breadcrumb 的实现缺陷；但在用户要求统一正式箭头图标后，该样例应迁移到 `RightOutlined` 并核对尺寸/基线。
- 默认 Breadcrumb `/` 与上游一致，应保留。Space 的文本分隔、OTP 分隔、Badge `99+`、AvatarGroup `+N`、Password/OTP 遮罩、Form `*`、Descriptions `:`、数值正负号和用户自定义 Rate character 不属于操作图标残留。

### 扫描结论与后续检查

- `099` 已将 DatePicker 日期导航、Menu 展开、Pagination 前后页/跳段、Table 排序改为内置 Icon。它们不是仍未替换的字符残留；Table 的选型问题另见 TUNE-VIS-003。
- 本轮已扫描 `src/component/**`、`include/ryn/**` 及 Gallery 的短符号字面量并核对角色。InputNumber 步进、Tree switcher/check、Steps 状态、Modal/Drawer close、Search、Password、Empty/Result、Alert、Spin、Tag close 已有正式图标；Checkbox/Radio 的原生几何也不是字符占位。此扫描不能保证所有动态传入文本都没有符号占位。
- 扫描范围必须含公开组件、组合控件、窗口层和可操作 Gallery。重点检查箭头/展开收起、关闭/清除、搜索/密码可见、数值步进、排序、日历翻页、状态/loading/确认；不能仅检索 `>` 和 `^`。
- 对命中逐个检查语义和实际挂载路径。普通数字/日期、负号/公式、文本省略、用户自定义 Rate character、明确展示符号的参考说明，不自动改成图标。原生 vector/path 几何不是文本占位。
- 统一使用既有 `Icon` / `IconName` 与锁定资源，按照上游选型、基线、尺寸、stroke/fill、方向及状态颜色核对；不得引入新的符号字体依赖或复制另一套私有 SVG。
- 问号/黑菱形要保留最小出现位置和安全文本码点：分别检查 UTF-8 解码是否产生 U+FFFD、字体是否缺 glyph、IconName 是否指向正确 bundled glyph。已有中文可显示不能排除个别图标缺字。
- 正式 Icon 已有独立 bundled font 路径（[text_component.cpp](../../src/component/text_component.cpp) L474–475、[text_scene_service.cpp](../../src/text/text_scene_service.cpp) L118–141），加载失败会报错；本轮没有发现该路径退化成系统字体问号的实证。848 图标 coverage 测试已存在，但本轮未重跑。普通 Text 的字体链 replacement 路径应单独诊断。
- 验收覆盖正常/hover/active/disabled/loading、三尺寸和主题，1/1.25/1.5/2 scale，实际生产字体链及 bundled icon；截图与图标来源断言结合，不能只凭字符串扫描通过。

## 7. Theme 与验收缺口

**TUNE-THEME-001：全局配置与组件 Token 映射需要收口。** [ThemeConfig](../../include/ryn/theme.hpp) L1699–1753 提供组件 Token、算法和 inherit，ThemeProps L2502–2513 只有 config 入口。全局 componentSize/componentDisabled/locale 的继承及显式值优先级尚需补齐；已有 Flex/Space、选择控件和文本方向能力不能被误写为“整个库没有 RTL”。新组件的首版 RTL 排除项需逐项处理。对照 [固定版本 ConfigProvider](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/config-provider/index.en-US.md#L46)。

Token catalog 的 `runtime` 数量是生成映射分类，不是组件完成率。比如 Menu 图标间距尚被归为 web-only，说明本轮必须检查原生可映射项的分类和实际消费；不能把全目录没有 missing 当作视觉对齐通过。

**TUNE-EVIDENCE-001：几何/Recording 通过不等于像素与动画通过。** 需要补各组件实际状态的截图和中间帧对照。保留 [open-issues](../open-issues.md) 的 Windows 字形 raster、Linux Fontconfig/Wayland、Gallery 视觉等独立证据边界；旧色块遮字已经修复但待完整验收的项目不能重报为当前仍有遮挡。不得把历史数据与当前更大 Gallery 的数据直接混合计算改善比例。

## 8. 分阶段执行顺序

每阶段建立自己的 OpenSpec proposal/design/spec/tasks，按当前规则进入 apply；小阶段通过相应验收即 commit、fetch/rebase（安全且需要时）、push、核对远端。以下 checkbox 是未来任务，本次文档提交不勾选实现项。

### 阶段 A 固定可重复基线

- [ ] 排除 100 的在改文件；100 发布后针对受影响合同做交叉回归，通过后不无故重复；新的审查基线另记 SHA。
- [ ] 固定参考 Ant Design 6.6.5、字体、主题、尺寸、文本、日期、容器宽高及缩放。准备少量一一对应的典型场景，逐阶段扩展，不先建设庞大平台。
- [ ] 记录 Release 二进制/源码 SHA、preset、机器/GPU、刷新率、scale、窗口、数据量、冷/热状态；性能采样时不并发构建。
- [ ] 给图标问题建立逐处清单与码点/资源来源证据；本报告未现场复现的几何边界先重现再改。

### 阶段 B 优先修复局部更新成本

- [ ] B1：TUNE-PERF-001、002，等长 glyph 更新和 buffer 容量策略。
- [ ] B2：TUNE-PERF-003、004，Tree hover/焦点与 Table 选择/排序投影隔离。
- [ ] B3：测量 TUNE-PERF-005、006；有证据再做订阅/dirty 合并最小改造。
- [ ] 保持受控状态、草稿、节点/ref 身份和失败恢复；每个修复有自己的可重复输入和前后计数，不用整体 Gallery 波动代替归因。

### 阶段 C 先修布局、绘制与图标

- [ ] C1：TUNE-ICON-001、002、003 与 TUNE-VIS-001、002、003；先修复已确认字符残留，统一真实 Token 公式与正式图标，再做截图对比。
- [ ] C2：TUNE-VIS-004、005，小高度日期可达、极窄 Tabs；扩展同类 popup/footer/长标题/富内容边界。
- [ ] C3：TUNE-VIS-006、007，Progress alpha 正确性、Drawer 四方向阴影。
- [ ] 全 50 条目逐组核对默认、hover、active、focus、selected、disabled、loading、error/warning（适用者），以及文本/图标基线、边框连接、clip 与 hit bounds。

### 阶段 D 按交互家族补动画

- [ ] D1：TUNE-MOTION-001；Checkbox/Radio 标记、Switch 材质、现有 Button 动效逐帧对照。
- [ ] D2：TUNE-MOTION-002；先 Tooltip/Popover/Popconfirm，再 Modal/Drawer，分别验证焦点与事件时序。
- [ ] D3：TUNE-MOTION-003；Collapse、Segmented，再 Tabs indicator/Tree 展开等几何动画。
- [ ] D4：TUNE-MOTION-004；Notice Loading/计时/堆叠，再 Badge/Tag/Alert/Skeleton/Progress 各自的反馈动画。
- [ ] 每项有 motion=false/reduced、反向 retarget、隐藏/裁剪/失活/销毁停止、最终 idle；只补需求所需通道，复用现有 AnimationRuntime。

### 阶段 E 既有组件原生能力补齐

- [ ] E1：Theme 全局尺寸/禁用/locale 与继承优先级；核对各组件 RTL，保留已完成的局部方向能力。
- [ ] E2：常用业务流优先：Select multiple/tags、TreeSelect 搜索/勾选、Form 布局/动态字段/校验、Table 过滤/固定列、日期时间范围/locale。
- [ ] E3：根据实际数据规模补虚拟化、动态拓扑、图片及其余矩阵缺口；不能因“以后会虚拟化”跳过阶段 B 的局部更新修复。
- [ ] 每个缺口选择“已实现并有证据”或“有明确桌面适配决定及影响”；不能整体把 partial 改为 implemented，也不把 Web-only 兼容 API 作为必要目标。

### 阶段 F 独立平台验收与收口

#### 平台通用

- [ ] 在一个正式平台/preset 执行适用的 Debug/Release HEADLESS/Recording、组件合同、确定时钟和性能计数；不要求 Windows/Linux 重复同一逻辑合同。
- [ ] 每一项问题附修复 commit、测试/benchmark/截图路径、结果、残余限制；审查覆盖与修复完成分列。

#### Windows

- [ ] MSVC、D3D12/DXIL、系统字体、真实 display scale、短窗口/窄宽、键鼠/系统 IME、截图/动画和用户滚轮体验。
- [ ] Release 在实际高刷屏记录 present cadence、CPU 分段、p50/p95/p99/最大值和掉帧；单独说明自动滚动与真实滚轮。

#### Linux

- [ ] 实际 Linux 原生 Wayland/Vulkan/SPIR-V、Fontconfig、分数缩放、IME/候选窗、键鼠和视觉动画。
- [ ] XWayland 与原生 Wayland 分别记录；没有对应机器证据保持未完成，不回退 Windows 已完成项。

## 9. 验收口径

性能必须同时看结果与原因：frame/present p95/p99、输入到可见反馈、component 执行、measure/layout 次数、visited nodes、glyph reshape/raster、heap allocations、buffer reallocations、upload bytes/ranges、composer rebuild 和 idle wake。常见刷新周期约为 60Hz 16.67ms、120Hz 8.33ms、144Hz 6.94ms、240Hz 4.17ms，仅作预算换算；CPU 平均值低于预算不等于显示器无掉帧。各阶段冻结自己的目标/容差并同机至少重复三轮，保留全部结果，不事后挑最好一轮。

视觉比较固定同版配方和相同逻辑条件；优先精确断言几何、Token、图标来源及状态，再用 GPU 图像验证 raster/合成。字体平台差异单独记录，不用全图宽容阈值吞掉错位、裁剪或丢图标。动画至少覆盖起点、关键中间态、终点及快速反向；最终态正确不代表过程正确。

新增测试应命中真实缺陷或当前验收缺口，避免复制实现公式自证。文档/规划校验使用 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`；C++/HLSL 修改另按项目规则执行格式检查及相应 build/test。禁止用目录数量、启动 smoke 或历史 CLEAN 代替本阶段验收。

## 10. 本轮交付状态

本轮交付为审查结果、全量范围矩阵和分阶段整改入口。代码修复、当前基线性能测量、逐组件 GPU 截图/动画及分平台验收尚未执行；本文未把任何待修复项标为完成。

本轮轻量核对：Ant Design 6.6.5 baseline checker（含 self-test）、`openspec doctor --json`、strict validate（100/100）、`git diff --check` 均通过；三份文档的本地链接存在性检查通过，覆盖表为 50 行、问题 ID 共 22 项，未来实现 checkbox 均未勾选。独立文档复核及关键源码抽查未发现阻止交付的问题。以上均不包含新的 C++ 构建、性能或平台视觉验收。

## 11. 实施跟踪（2026-10-29 启动）

用户已明确要求实施本报告并允许多 agent。以上第 1–10 节保留原审查事实；本节持续记录实施，不把原报告改写为已修复。实施起点为 `dd3e71f775fb8d4f1f46aeeff5ea444ba816e54d`；截至 2026-10-30，099 四控件图标、100 指定滚动优化及 101 局部字形更新已完成各自验收与独立复核。100 容量上限 P2 已修复，整页 240 Hz 体验仍待验。

两个首批 change 进入 apply：

- [101 局部 glyph 更新](../../openspec/changes/101-20261029-optimize-local-glyph-updates/tasks.md)：等长替换及容量增长补验。GPU 上限小修由父线程在 100 内处理，避免与 worker 重复修改。
- [102 Tree/Table 状态投影](../../openspec/changes/102-20261029-isolate-tree-table-state-projection/tasks.md)：Tree hover/focus 与 Table 选择/排序隔离。

隔离 worker 先完成各自源码、测试与可重复输入；父线程集中安排正式构建与基准，避免并发编译/性能测试占用电脑。每个完成项必须关联实现 SHA 和实际证据。后续阶段按第 8 节继续，不把尚未启动项从范围中删除，不以规划或 handoff 勾选完成。

| 问题 | 当前实施状态 |
| --- | --- |
| TUNE-PERF-001 | 已修复并独审 CLEAN：76c469bd；MSVC D/R 回归、三轮基准与证据边界见 101/evidence/01-main-validation.md、02-final-review.md |
| TUNE-PERF-002 | 已修复并独审 CLEAN：bced2df3、76c469bd；连续增长、变化设备能力及失败重试通过，见 100/evidence/04-final-review.md |
| TUNE-PERF-003 | Tree 已修复并独审 CLEAN：f595211e；MSVC D/R、三轮局部交互与故障重试见 102/evidence/01-tree-main.md、02-tree-review.md；不代表整页滚动达标 |
| TUNE-PERF-004 | Table 投影缓存/分页复用已完成 MSVC D/R 回归与三轮基准，待独审；见 102/evidence/03-table-main.md。仍有逐行状态与 scene 成本，不代表整体常数更新或高刷达标 |
| TUNE-PERF-005 | 阶段 B3：待高扇出测量后最小修复 |
| TUNE-PERF-006 | 阶段 B3：待离散 ranges 测量与合并优化 |
| TUNE-VIS-001 | 阶段 C1：Menu Token 待实施 |
| TUNE-VIS-002 | 阶段 C1：DatePicker 配方待实施 |
| TUNE-VIS-003 | 阶段 C1：Table 双 caret 待实施；099 仅完成字符替换 |
| TUNE-VIS-004 | 阶段 C2：短窗口日期/footer 可达待实施 |
| TUNE-VIS-005 | 阶段 C2：极窄 Tabs 待实施 |
| TUNE-VIS-006 | 阶段 C3：Progress alpha 待实施 |
| TUNE-VIS-007 | 阶段 C3：Drawer 阴影待实施 |
| TUNE-MOTION-001 | 阶段 D1：选择控件状态动画待实施 |
| TUNE-MOTION-002 | 阶段 D2：浮层进退场待实施 |
| TUNE-MOTION-003 | 阶段 D3：几何动画待实施 |
| TUNE-MOTION-004 | 阶段 D4：Notice 计时/反馈动画待实施 |
| TUNE-ICON-001 | 阶段 C1：Progress 状态 Icon 待实施 |
| TUNE-ICON-002 | 阶段 C1：Gallery Loading slot 待实施 |
| TUNE-ICON-003 | 阶段 C1：Gallery Breadcrumb 自定义箭头待实施 |
| TUNE-THEME-001 | 阶段 E1：全局配置与优先级待实施 |
| TUNE-EVIDENCE-001 | 分阶段积累实际证据；50 项状态矩阵、原生动画和各平台结果分别跟踪 |

阶段 E2/E3 的 multiple/tags、搜索/勾选、Form 布局/动态字段/校验、Table 过滤/固定列、日期时间范围/locale、动态拓扑/虚拟化/图片等补齐仍在待办范围，后续按实际业务流拆分。Linux、系统 IME/AT、用户高刷体验保持独立待验；本次规划不声称这些边界已通过。
