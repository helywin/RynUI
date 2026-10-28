# 原生组件收尾清单

2026-10-02：用户明确要求补齐已有组件的原生桌面功能；DOM/CSS/HTML、Web 兼容别名不移植。此清单记录执行范围，功能完成与平台验收分开；不通过修改目录状态代替实现。

| 组件 | 已观察到的原生功能缺口 | 执行依赖/收尾方式 |
| --- | --- | --- |
| Tooltip（新增共用基础） | 040/043 已实现文字/富标题、hover/focus/click/context menu/组合/controlled/手动、Escape/外部关闭、主题、12 方位、pointAtCenter 和向量箭头 | 原生功能 implemented；043 平台通用和 Windows/Linux 原生证据独立记录 |
| Slider / RangeSlider / MultiSlider | 041/042 已补齐原生 marks/dots/提示、动态多端点、整段拖动/编辑、逐端点禁用与 ref | 原生功能 implemented；042 Windows/GPU 与 Linux 验收独立记录 |
| Typography | 057 已接入复制/编辑/实际截断提示，编辑态复用 TextArea/autoSize 并保留完整换行 | 功能与平台通用、Windows/Linux 原生验收独立记录；富文本仍未覆盖 |
| Divider | 045 补齐 dotted、尺寸、Start/End 与 scoped RTL、逻辑长度间距；重新审计发现这些是实际原生缺口 | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Button | 044 已实现颜色/六变体/ghost/真实虚线、保留 icon/loading slots、形状/block、ref/autoFocus/延迟加载与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Icon | 051 已实现完整 848 离线图标、三类/逐层双色、typed 自定义贝塞尔与 holes、保留 rotate/spin、Theme/motion/DPI 与窗口缓存清理 | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Flex | 049 已补齐默认 Stretch、真实字体与控件基线/align-self、WrapReverse、LTR/RTL、物理 Left/Right 和原生别名/弹性数值映射；上游无 responsive API | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Space | 050 已实现 Auto/Baseline、orientation/RTL、富 separator/split、Compact H/V/三尺寸/block/nested、Button/Input/Password/Search/RadioButton 连接外角与状态边、四变体 Addon；上游无 responsive size API | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Checkbox | 047 已实现三类值、多选 Group、动态 options、最近组/skipGroup、RTL/ref/autoFocus/onClick、独立主题与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Radio | 048 已实现三类值、动态 options、最近 typed Group、单一 Tab 入口/方向键、RadioButton、outline/solid、三尺寸/block/H/V/RTL、ref/autoFocus/onClick、独立主题与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Switch | 046 已实现保留状态文字/图标、RTL、ref/autoFocus/onClick、加载透明度、主题/阴影、按压与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Input / Search / Password / TextArea / OTP | 052 单行特性、053 多行/autoSize/scroll/resize、054 OTP 分格/formatter/mask/导航/动态长度已实现。仍待 visual bidi | 后续独立 change；保留 Unicode/IME/clipboard 合同，平台证据独立记录 |
| Theme（ConfigProvider 原生映射） | 原生公共配置覆盖需要审核 | 作为 Theme 收尾，Web ConfigProvider API 不照抄 |

来源：`examples/token_gallery/generated_ant_design_reference_catalog.inc`、现有公开 headers、changes 003–039 的 tasks。每项开发前再次检查对应源码与锁定 Ant Design 6.6.5 合同，后续清单随已验证实现更新。

平台缺口：历史 Windows tasks 存在系统 IME/人工视觉确认项，Linux 专属 tasks 需实际 native Linux 机器。已通过 Windows 与平台通用项保持完成；没有实际证据的 checkbox 保持未勾选。提交与推送遵循当前协作规则，不自动 archive 或将历史 evidence 升级为当前验收。

## 常用业务组件优先级（2026-10-23 用户指示）

优先补齐能组合业务页面的常用组件，Theme 全局配置与已有组件长尾 API 后置。每项先完成真实可用的首版和对应验证，明确未覆盖范围；规划不计入实现。每完成一次 commit 立即 push 并核验远端 SHA。

| 顺序 | 组件 | 首轮可用目标 |
| --- | --- | --- |
| 1 | Select | 058已实现单选/搜索/清空/动态选项/键盘与窗口层，平台通用验证已通过；Gallery与原生证据独立记录，multiple/tags后续扩展 |
| 2 | Tabs、Card | 059 Card 已实现五区域内容容器；060 Tabs 已实现保留面板/草稿、受控选择、Line/Card、三尺寸/上下/居中、键盘与溢出导航；共同验证已通过，Gallery/平台证据独立记录 |
| 3 | Modal、Drawer、Popconfirm | 061 Modal、062 Drawer、064 Popconfirm 已提供可用首版和实际 Gallery：保留插槽、受控请求、焦点/输入边界、正文滚动与子浮层；Modal Windows 原生矩阵已通过，其他原生平台项独立记录 |
| 4 | Tag、Badge、Alert、Spin、Progress | 063 Tag/CheckableTag 与 Badge、065 Alert/Spin/Progress 已提供保留内容、受控事件、状态色/计数、加载屏蔽和三种确定进度；共同 Debug/Release 验证通过，069 补齐交互 Gallery，原生视觉独立记录 |
| 5 | Pagination、Table | 066 已实现保留键控行、typed 列/插槽、受控排序/Checkbox 选择、有限滚动和可组合 Pagination；072 提供实际数据编辑/排序/选择/翻页 Gallery，原生平台验收独立记录 |
| 6 | Menu、Dropdown | 067 已实现保留菜单、受控选择/Inline 子菜单、键盘/滚动、点击/右键 Dropdown、子浮层与焦点恢复；072 提供导航/命令 Gallery，平台证据独立记录 |
| 7 | InputNumber、DatePicker、TimePicker、Form | 068 InputNumber 已实现数值草稿、受控回写、范围/精度/步长、按钮/键盘和 IME 门禁，072 提供实际 Gallery；075 Form 已提供纵向 typed 字段、显式绑定、同步校验、提交/重置与保留反馈，见 [Form API](form.md)。077 DatePicker 已提供公历值、ISO 输入、真实月历、范围/禁用与保留 footer，见 [DatePicker API](date-picker.md)。078 TimePicker 已提供严格 24 小时输入、保留时分秒列、确认/即时提交、规则、受控开合及 String Form 同值 reset，见 [TimePicker API](time-picker.md)，Core 组合通过独立 Astra R4。079 Form/Date/Time Gallery **4/4 完成**，独立 Astra R4、完整 frame 与目录 contracts **9/9**、Direct3D 12 / DXIL 七帧原生启动通过，见 [最终证据](../openspec/changes/079-20261025-add-form-and-picker-gallery/evidence/final-main-acceptance.md)；组件人工视觉、完整 GPU/DPI、Linux、系统 IME/辅助技术仍独立记录 |
| 8 | Collapse、Segmented | 070/071 提供保留面板与固定分段选项；074 Gallery 已补齐受控设置、保留 Input/Select、独立 extra、共享横纵选择、等宽/富标签和响应式标签/禁用。聚焦场景、完整 inventory/Recording 与目录 contracts 9/9、Windows D3D12/DXIL 启动 smoke 已通过，见 [074 证据](../openspec/changes/074-20261024-add-collapse-and-segmented-gallery/evidence/panel-selection-gallery.md)；组件 GPU/人工视觉、系统 IME 与 Linux 仍独立记录 |
| 9 | Empty、Result | 073 已实现静态原生图示/语义状态/403/404/500、响应式说明/标题、保留 slots/实际 Button/Input 与 Theme；集成后 headless Debug/Release 148/148 通过，见 [073 证据](../openspec/changes/073-20261024-add-native-empty-and-result/evidence/integration-c3190b0/closure.md)。076 Gallery 已补齐创建/图示切换、七状态循环、保留草稿、自定义图示和真实目录跳转；完整 Gallery/目录 contracts 9/9、文档 model/viewport、Windows D3D12/DXIL 启动 smoke 与 Astra R2 复核通过，见 [076 证据](../openspec/changes/076-20261024-add-empty-and-result-gallery/evidence/states-gallery.md)。大型 SVG 插画、网络/动画/Web/a11y 桥接未覆盖，平台证据独立记录 |
| 10 | Breadcrumb、Steps | 080 已实现固定 keyed 层级/步骤、typed 富 slots、原生键盘/指针激活、Steps 受控 current 与四状态/连接线、隐藏恢复和 Theme；富子控件输入、焦点轮廓与 input block 销毁通过独立 Astra R3。082 的真实目录跳转、层级/步骤切换、富子控件编辑、显隐/禁用恢复及三主题双宽 Gallery 通过独立 Astra R1，并纳入 079 统一完整 frame/contracts **9/9** 与 Direct3D 12 / DXIL 启动，见 [Breadcrumb API](breadcrumb.md)、[Steps API](steps.md)、[082 证据](../openspec/changes/082-20261025-add-navigation-gallery/evidence/02-main-and-review.md)。组件人工视觉、完整 GPU/DPI、Linux、系统 IME/辅助技术仍独立记录 |
| 11 | Skeleton | 083 已提供静态标题/段落/avatar、独立 Avatar/Button/Input、retained loading 输入门禁和 Theme，MSVC headless Debug/Release 各 51/51 及独立 Astra R1 通过。086 已提供 loading、正文编辑/分享/发布、占位切换及草稿/refs 恢复的实际 Gallery，三主题双宽与独立 Astra R1 通过，见 [Skeleton API](skeleton.md)、[086 集成记录](../openspec/changes/086-20261025-add-descriptions-and-skeleton-gallery/evidence/02-main-and-review.md)。active/shimmer、Image/Node 及组件平台视觉验收仍未覆盖；本批完整 Gallery 与启动见下方统一记录 |
| 12 | Descriptions | 081 已提供固定 keyed 多列/span、横纵标签、边框/三尺寸、保留富 slots、分区内容裁剪和外扩焦点装饰；独立 Astra R3 CLEAN、父分支组合测试 44/44 和 inventory 通过。086 已提供真实编辑/选择/保存、列数/跨列/方向/尺寸/显隐恢复及目录直达，三主题双宽与独立 Astra R1 通过，见 [Descriptions API](descriptions.md)、[081 证据](../openspec/changes/081-20261025-add-native-descriptions/evidence/main-publication.md)、[086 集成记录](../openspec/changes/086-20261025-add-descriptions-and-skeleton-gallery/evidence/02-main-and-review.md)。动态拓扑、自动断点、RTL 及组件平台视觉验收仍未覆盖；本批完整 Gallery 与启动见下方统一记录 |
| 13 | Popover | 084 已提供 typed 交互富内容、受控开合、hover/focus/click 组合与延时、十二方位/箭头、原生输入和子浮层归属；六项 R1 问题修复后通过独立 Astra R2，父 main 新构建回归 52/52 与 inventory 通过，见 [Popover API](popover.md)、[集成证据](../openspec/changes/084-20261025-add-native-popover/evidence/main-integration.md)。091 已接通目录直达、备注编辑/保存、定位/箭头及禁用恢复的实际 Gallery；contextMenu、动画、RTL、内置内容滚动与平台专属验收未覆盖 |
| 14 | Message / Notification | 085 已提供窗口内 holder/ref、keyed 更新与关闭、五种状态、四角通知、保留富内容、数量限制及单调时钟自动关闭；独立 Astra R3 CLEAN，父 main 与 Popover 的新构建组合回归 81/81 及 inventory 通过，见 [API](message-notification.md)、[集成证据](../openspec/changes/085-20261025-add-native-message-and-notification/evidence/main-integration.md)。091 已接通打开、同 key 更新、关闭/清空、四角/状态及富备注保留编辑的实际 Gallery；loading 为静态图标，全局 service、动画、暂停计时和平台专属验收未覆盖 |
| 15 | Timeline | 087 已提供竖向固定 keyed 富条目、Start/End/Alternate、正反序、显隐、静态 loading 与分批虚线轨迹；组件 Astra R2 和最终共享合并 Astra R2 CLEAN，父新构建 Debug 96/96、MSVC headless Release 53/53 通过，见 [Timeline API](timeline.md)、[集成证据](../openspec/changes/087-20261025-add-native-timeline/evidence/main-integration.md)。091 已接通记录编辑/保存、位置/顺序/loading 及显隐恢复的实际 Gallery；横向、动态拓扑、titleSpan、RTL、动画与平台专属验收未覆盖 |
| 16 | Avatar / AvatarGroup | 088 已提供文字/图标、真实字体测量 auto-fit、retained slots、固定 keyed Group、overlap/max.count 及换行/滚动溢出 Popover；独立 Astra R2 未发现可达 P1/P2，main 新构建 MSVC headless Debug/Release 各 18/18 通过，见 [Avatar API](avatar.md)、[main 验收](../openspec/changes/088-20261026-add-native-avatar/main-integration.md)。093 已提供成员编辑、auto-fit、Group 溢出滚动与富草稿 Gallery，父验收见下方统一记录；图片管线、Group 任意富 slot、responsive 与平台专属验收未覆盖 |
| 17 | Rate | 090 已提供受控/默认评分、真实半星、清空、文字/图标、动态数量、原生键盘及保留富 Tooltip；字号范围、零尺寸/输入清理、鼠标过渡与动态 Tab 顺序修复后通过独立 Astra R3。与 Avatar 合并后的 main 新构建 MSVC headless Debug/Release 各 29/29 通过，见 [Rate API](rate.md)、[main 验收](../openspec/changes/090-20261026-add-native-rate/evidence/main-integration.md)。093 已提供半星/清空、键盘、门禁恢复与富 Tooltip 草稿 Gallery，父验收见下方统一记录；任意重复原生 character 及组件平台专属验收未覆盖 |
| 18 | Statistic | 089 已提供 typed 数值/String、精度截断补零、分隔符/formatter、保留标题/数值/前后缀与 Skeleton loading；正负零更新、首次挂载/私有结构退役、最新属性交接和局部异常回滚经独立 Astra R7 复核。与 Avatar/Rate 合并后的 main 新构建 MSVC headless Debug/Release 各 45/45 通过，见 [Statistic API](statistic.md)、[main 验收](../openspec/changes/089-20261026-add-native-statistic/evidence/main-integration.md)。093 已提供 typed 数值、精度/格式化、loading 与保留富输入 Gallery，父验收见下方统一记录；Timer 系列、动画、locale 和组件平台专属验收未覆盖 |
| 19 | AutoComplete | 092 已提供自由 String 输入、动态键控/富建议、受控值与开合、backfill、真实键盘/指针与 Form 同值 reset；异常退役、同帧后代焦点与子浮层恢复经独立 Astra R4，和 Tree/已发布组件合并后 main 新构建 MSVC headless Debug/Release 各 98/98，组合源码审查 CLEAN。见 [API](auto-complete.md)、[组合验收](../openspec/changes/094-20261026-add-native-tree/evidence/main-integration.md)。096 已提供联系人建议、富备注、受控拒绝/编辑恢复与 Form String 同值 reset 的实际 Gallery，三主题双宽和独立 Astra R2 通过，见下方集成记录；网络异步、分组、虚拟化、动画、RTL 与平台专属验收未覆盖 |
| 20 | Tree | 094 已提供固定键控层级、受控展开/单选、非 strict 级联勾选、禁用隔离、保留富 slots、roving 键盘与有限滚动；挂载回滚、零面积清理、checkbox 位移和嵌套滚动修复经独立 Astra R3，父组合新构建和审查同上。见 [API](tree.md)、[组合验收](../openspec/changes/094-20261026-add-native-tree/evidence/main-integration.md)。096 已提供项目层级操作、全选/半选反馈、富备注与显隐禁用恢复的实际 Gallery，三主题双宽和独立 Astra R2 通过；首末行焦点修复另经新构建 Debug/Release 各 26/26。见下方集成记录；动态拓扑、多选、异步、虚拟化、拖放、连接线、RTL 与平台专属验收未覆盖 |
| 21 | TreeSelect | 097 已实现固定 String key 单选、受控值/开合/展开、clear、readOnly/disabled/visible、真实 retained Input/Popover/Tree、键盘/有限列表、Form 与独立 Theme。父 MSVC headless 定向 Debug/Release 各 12/12 已通过，组合回归及独立 Astra 收口尚在进行，见 [API](tree-select.md)、[阶段任务](../openspec/changes/097-20261028-add-native-tree-select/tasks.md)。交互 Gallery、多选、搜索、异步、动态拓扑、富节点/自定义 popup、虚拟化、动画、RTL/Web/AT 及原生分平台验收不在首版范围 |

084–087 已收尾：冻结源码 `fbce29a0` 的完整 Gallery/目录合同 **9/9** 与 Windows Direct3D 12 / DXIL 七帧启动通过；组合 Debug **96/96**、headless Release **53/53**，相关 Astra 复核 CLEAN。见 [统一最终验收](../openspec/changes/086-20261025-add-descriptions-and-skeleton-gallery/evidence/03-frozen-batch-final.md)。完整组件 GPU/人工视觉、Linux、系统 IME/AT 不据此宣称通过，首版排除项保持。

091 新增流程通过三主题双宽定向 Recording、实际 inventory 和八项目录 contracts；通知/Timeline 焦点装饰修复后独立 Astra R2 无新 P1/P2，Date/Time 与 Descriptions/Skeleton 代表回归通过，见 [091 实施记录](../openspec/changes/091-20261026-add-overlay-and-timeline-gallery/evidence/01-gallery.md)、[修复与复核](../openspec/changes/091-20261026-add-overlay-and-timeline-gallery/evidence/02-review-and-regressions.md)。新流程的完整 Gallery/native 统一验收尚未执行，不能沿用上一批的完整 frame 结果。

093 成员/指标/评分 Gallery 已完成 main 新构建、三主题双宽 focused、精确 inventory、八项合同及旧浮层/Timeline 代表；独立 Astra 组合源码审查 CLEAN。实测 419 IDs、302 live samples，详见 [父集成证据](../openspec/changes/093-20261026-add-member-and-rating-gallery/evidence/03-main-integration.md)。完整 Gallery/native 统一验收尚未执行，Recording 不计为 GPU/人工视觉、Linux、系统 IME 或 AT。

096 联系人/项目树 Gallery 已完成新构建、三主题双宽 focused、精确 inventory、八项合同及最终旧成员/指标/评分代表；R1 两项实际复现并修复，独立 Astra R2 CLEAN。实测 441 IDs、305 live samples、AutoComplete 2、Tree 1，见 [096 实施与验收](../openspec/changes/096-20261028-add-tree-and-autocomplete-gallery/evidence/implementation.md)。Tree 首末行焦点在 094 单独验证发布后作为冻结父，见 [边界焦点证据](../openspec/changes/094-20261026-add-native-tree/evidence/boundary-focus.md)。完整 Gallery/native 统一验收仍未执行，平台边界保持。

以上记录各项实际进展与剩余范围；Gallery support status 按对应原生实现和验收证据单独维护。现有 Button、Input 家族、布局、选择控件、Typography 等已实现能力继续保持可用。
