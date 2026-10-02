# 原生组件收尾清单

2026-10-02：用户明确要求补齐已有组件的原生桌面功能；DOM/CSS/HTML、Web 兼容别名不移植。此清单记录执行范围，功能完成与平台验收分开；不通过修改目录状态代替实现。

| 组件 | 已观察到的原生功能缺口 | 执行依赖/收尾方式 |
| --- | --- | --- |
| Tooltip（新增共用基础） | 040 已实现窗口文字浮层、hover/focus/controlled/手动、Escape、主题和 12 种定位；rich content、click/context menu、arrow pointAtCenter 待扩展 | 平台通用与 Windows 原生证据已保存；Linux 原生独立待验 |
| Slider / RangeSlider | 041 已实现 marks/dots、值 Tooltip、included/离散点；整段拖动、多端点与动态端点继续实施 | 保持 partial；沿用 retained scene 与独立焦点 |
| Typography | 复制/编辑/省略的提示；多行编辑依赖 TextArea | Tooltip 后补提示，TextArea 后补多行编辑 |
| Divider | 目录只因 Web API 标 partial，原生合同已有实现 | 核对原生覆盖与测试后更新状态；Linux 验收独立 pending |
| Button | dashed/link/ghost、preset color、icon placement、wave | 独立变体/图标/交互 change |
| Icon | 完整离线图标集、filled/two-tone 与 typed 原生自定义图标 | 锁定来源/许可/生成器，复用 glyph atlas |
| Flex | 公开 responsive breakpoint 值 | 窗口宽度响应合同，旧布局/API 兼容 |
| Space | split、Compact、响应尺寸 | typed slots 与共同控件边界 |
| Checkbox | CheckboxGroup/options | typed value、reactive selection、Group disabled 与键盘/主题 |
| Radio | 已有组件和 group，目录误标 planned | 审核 API 与合同后同步目录，补缺失项 |
| Switch | checked/unchecked 内容与图标 | retained typed slots、主题、loading/disabled |
| Input / Search / Password | TextArea、OTP、Search 图标/Compact；visual bidi、系统输入属性 | 分独立 change；保留编辑会话/Unicode/IME/clipboard 合同 |
| Theme（ConfigProvider 原生映射） | 原生公共配置覆盖需要审核 | 作为 Theme 收尾，Web ConfigProvider API 不照抄 |

来源：`examples/token_gallery/generated_ant_design_reference_catalog.inc`、现有公开 headers、changes 003–039 的 tasks。每项开发前再次检查对应源码与锁定 Ant Design 6.6.5 合同，后续清单随已验证实现更新。

平台缺口：历史 Windows tasks 存在系统 IME/人工视觉确认项，Linux 专属 tasks 需实际 native Linux 机器。已通过 Windows 与平台通用项保持完成；没有实际证据的 checkbox 保持未勾选。此清单不自动 push、archive 或将历史 evidence 升级为当前验收。
