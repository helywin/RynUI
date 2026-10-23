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
| 5 | Pagination、Table | 数据分页、表头/行/列、选择/排序等常用数据浏览流程 |
| 6 | Menu、Dropdown | 应用导航与操作菜单，复用已验证的窗口层行为 |
| 7 | InputNumber、DatePicker、TimePicker、Form | 数值/日期/时间录入和表单验证组合 |

这些为待实现顺序，不能据此提升 Gallery support status；现有 Button、Input 家族、布局、选择控件、Typography 等已实现能力继续保持可用。
