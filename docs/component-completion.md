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

以上记录各项实际进展与剩余范围；Gallery support status 按对应原生实现和验收证据单独维护。现有 Button、Input 家族、布局、选择控件、Typography 等已实现能力继续保持可用。
