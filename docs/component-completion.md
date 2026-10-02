# 原生组件收尾清单

2026-10-02：用户明确要求补齐已有组件的原生桌面功能；DOM/CSS/HTML、Web 兼容别名不移植。此清单记录执行范围，功能完成与平台验收分开；不通过修改目录状态代替实现。

| 组件 | 已观察到的原生功能缺口 | 执行依赖/收尾方式 |
| --- | --- | --- |
| Tooltip（新增共用基础） | 040/043 已实现文字/富标题、hover/focus/click/context menu/组合/controlled/手动、Escape/外部关闭、主题、12 方位、pointAtCenter 和向量箭头 | 原生功能 implemented；043 平台通用和 Windows/Linux 原生证据独立记录 |
| Slider / RangeSlider / MultiSlider | 041/042 已补齐原生 marks/dots/提示、动态多端点、整段拖动/编辑、逐端点禁用与 ref | 原生功能 implemented；042 Windows/GPU 与 Linux 验收独立记录 |
| Typography | 复制/编辑/省略的提示；多行编辑依赖 TextArea | Tooltip 后补提示，TextArea 后补多行编辑 |
| Divider | 045 补齐 dotted、尺寸、Start/End 与 scoped RTL、逻辑长度间距；重新审计发现这些是实际原生缺口 | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Button | 044 已实现颜色/六变体/ghost/真实虚线、保留 icon/loading slots、形状/block、ref/autoFocus/延迟加载与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Icon | 完整离线图标集、filled/two-tone 与 typed 原生自定义图标 | 锁定来源/许可/生成器，复用 glyph atlas |
| Flex | 049 已补齐默认 Stretch、真实字体与控件基线/align-self、WrapReverse、LTR/RTL、物理 Left/Right 和原生别名/弹性数值映射；上游无 responsive API | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Space | 050 已实现 Auto/Baseline、orientation/RTL、富 separator/split、Compact H/V/三尺寸/block/nested、Button/Input/Password/Search/RadioButton 连接外角与状态边、四变体 Addon；上游无 responsive size API | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Checkbox | 047 已实现三类值、多选 Group、动态 options、最近组/skipGroup、RTL/ref/autoFocus/onClick、独立主题与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Radio | 048 已实现三类值、动态 options、最近 typed Group、单一 Tab 入口/方向键、RadioButton、outline/solid、三尺寸/block/H/V/RTL、ref/autoFocus/onClick、独立主题与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Switch | 046 已实现保留状态文字/图标、RTL、ref/autoFocus/onClick、加载透明度、主题/阴影、按压与有限 wave | 原生功能 implemented；共同合同与 Windows/Linux 原生证据独立记录 |
| Input / Search / Password | TextArea、OTP、Search 图标/Compact；visual bidi、系统输入属性 | 分独立 change；保留编辑会话/Unicode/IME/clipboard 合同 |
| Theme（ConfigProvider 原生映射） | 原生公共配置覆盖需要审核 | 作为 Theme 收尾，Web ConfigProvider API 不照抄 |

来源：`examples/token_gallery/generated_ant_design_reference_catalog.inc`、现有公开 headers、changes 003–039 的 tasks。每项开发前再次检查对应源码与锁定 Ant Design 6.6.5 合同，后续清单随已验证实现更新。

平台缺口：历史 Windows tasks 存在系统 IME/人工视觉确认项，Linux 专属 tasks 需实际 native Linux 机器。已通过 Windows 与平台通用项保持完成；没有实际证据的 checkbox 保持未勾选。此清单不自动 push、archive 或将历史 evidence 升级为当前验收。
