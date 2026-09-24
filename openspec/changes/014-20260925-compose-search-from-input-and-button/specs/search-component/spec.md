# Spec Delta

## Purpose

为 RynUI 提供可与既有单行 Input、Button 和布局机制组合的 Search 控件，使桌面用户可通过 Enter 或按钮提交当前搜索文本，同时保持受控编辑、IME、主题和窗口生命周期的既有合同。

## ADDED Requirements

### Requirement: Search 必须提供 typed Props 与明确的受控模式
公开 `ryn::Search` SHALL 提供 typed `SearchProps`，覆盖 value/defaultValue、placeholder、size/status、disabled/readOnly、maxLength、loading、enterButton、onChange、onSearch 与外部 `LayoutStyle`；按钮内容 SHALL 可由独立 typed slot 提供。视觉样式 MUST 由既有 Theme 与 Input/Button Component Token 决定，不得通过通用视觉 `Modifier` 或 renderer 类型进入公开 API。value 与 defaultValue 同时指定 MUST 在挂载前拒绝，受控模式在同一 generation 内不得静默切换。

#### Scenario: 受控 Search
- **WHEN** value 绑定到 `Signal<String>` 且 onChange 将新值写回该 Signal
- **THEN** 输入区与按钮提交 SHALL 观察到同一 authoritative committed value，按钮内容与无关 sibling 不得因 value 更新重新挂载

#### Scenario: 非受控 Search
- **WHEN** caller 只提供 defaultValue 并编辑文本后点击按钮
- **THEN** Search SHALL 提交当前 committed value，而不是挂载时的 defaultValue

#### Scenario: 冲突 Props
- **WHEN** 同一 Search 同时提供 value 和 defaultValue
- **THEN** 挂载 MUST 失败并报告稳定错误，不得留下可交互 Input 或 Button

### Requirement: Search 提交必须遵守编辑与交互门禁
Input Enter 与按钮激活 SHALL 各自最多触发一次 onSearch，均传递当前 committed value 与 `SearchSource::Input`。Enter 在 composition 活跃时 SHALL 不触发搜索；按钮点击只提交最后一次 committed value，不包含未提交的 composition 文本。loading、disabled 或 readOnly 时两条路径 SHALL 不触发搜索；key repeat、key up、取消的指针手势与失效 generation MUST NOT 重复触发。当前未提供 clear 入口，不得虚构 clear 来源。

#### Scenario: Enter 与按钮提交
- **WHEN** 用户提交一次普通 Enter，之后单独点击一次 Search 按钮
- **THEN** onSearch SHALL 分别收到一次当前 committed value 和 `SearchSource::Input`

#### Scenario: IME 与 loading
- **WHEN** composition 尚未 commit 时按 Enter，或 Search 处于 loading 时按 Enter/点击按钮
- **THEN** 不得提交；composition 期间单独点击按钮时仅可提交最后一次 committed value，不得把 composition 临时文本当成已提交值

#### Scenario: 禁用与回调自毁
- **WHEN** disabled/readOnly 阻止搜索，或 onSearch 同步销毁 Search 所在子树
- **THEN** 不得残留 pointer capture、焦点、迟到 callback 或可见的旧 scene identity

### Requirement: Search 布局与视觉必须复用现有组件
Search SHALL 将单行 Input 与 Button 作为相邻的可交互子控件布局，尺寸与 Theme 分别沿用 Input/Button Token；enterButton=false 使用默认按钮语义，true 使用 primary 按钮语义。组件在小、中、大尺寸和 1.0/1.25/1.5/2.0 模拟 scale 下 SHALL 保持文字、Button、焦点效果、hit-test 和 clip 不重叠、不裁切；LayoutStyle 仅决定整体外部布局。

#### Scenario: 尺寸与主题切换
- **WHEN** Search 在 small/middle/large 之间切换并应用 Default/Dark/Compact 主题
- **THEN** Input 与 Button SHALL 同步使用对应高度和主题材料，颜色变化不得重新 shape 文本或重建稳定 scene topology

#### Scenario: 窄宽度
- **WHEN** 外部宽度缩小且输入值长于编辑区域
- **THEN** Input 文本 SHALL 按既有单行规则水平滚动或 clip，Button 仍可命中且不得与 Input 编辑区重叠

### Requirement: Search 不得复制窗口编辑设施
同窗 Search、普通 Input、Button 与 Switch/Checkbox SHALL 继续共享窗口级编辑、交互、scene、动画和帧同步设施。Search 自身状态更新 SHALL 限制在相关子控件的失效域；空闲时不得持续动画更新、scene 重建或 GPU 上传。

#### Scenario: 同窗生命周期
- **WHEN** Search 与普通 Input、Button、Switch/Checkbox 同时挂载并先后销毁或复用 slot
- **THEN** 原有控件 SHALL 保持各自 focus/编辑/按压行为，且不会创建第二个同平台 TextInputSessionHost

### Requirement: Search 验收必须区分平台通用与原生窗口行为
公开 API、提交状态机、headless scene/Theme/生命周期与 benchmark SHALL 在一个受支持正式 preset 验证一次；Win32 与原生 Linux Wayland 的系统输入、字体、DPI、GPU/shader 和真实视觉 MUST 各自保存独立 evidence，不得用一平台结果代替另一平台。

#### Scenario: Windows 已通过而 Linux 暂缓
- **WHEN** Windows MSVC、D3D12/DXIL 与真实窗口验收通过，原生 Linux Wayland 尚未执行
- **THEN** 平台通用与 Windows 任务可以完成，Linux 与最终跨平台收口项 MUST 保持未完成
