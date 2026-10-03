# Spec Delta

## Purpose

为桌面文字显示与输入提供 Unicode 双向段落、逻辑字符和视觉位置之间的共同合同，使阿拉伯语、希伯来语、数字及中英文混排在折行、光标、选择与 IME 中保持一致，并沿用既有 UTF-8、retained scene 和平台隔离边界。

## ADDED Requirements

### Requirement: Unicode paragraph direction
系统 SHALL 按 Unicode 17 UAX #9 分析段落，包括弱类型、数字、括号、embedding/override/isolate 与格式控制。Auto 使用首个强方向并在无强方向时回退 LTR，显式 LTR/RTL 固定基础方向；段落与实际视觉行独立处理。

#### Scenario: Mixed and isolated content
- **WHEN** 同段包含 Latin、Hebrew/Arabic、数字、成对括号及 isolate 控制字符
- **THEN** 系统按规范确定 levels 和视觉 runs，保留原 UTF-8 byte 索引，格式控制字符不显示 replacement glyph

#### Scenario: Direction and paragraph boundary
- **WHEN** 显式改变基础方向或包含独立换行段落
- **THEN** 各段重新分析，空段有效，Auto 与显式基础方向产生可验证结果

### Requirement: Shaping and line order
系统 SHALL 为同方向/script/font 的逻辑文本分段 shaping，保留 grapheme/ligature cluster 合同；按逻辑内容确定折行，再对每条实际行执行视觉重排。显示、测量、ellipsis 与 caret MUST 使用同一视觉 glyph 几何。

#### Scenario: Arabic font and script boundaries
- **WHEN** Arabic、Latin 与字体 fallback 在同段出现
- **THEN** Arabic 连写与括号镜像使用正确方向，数字保持自身次序；字体缺失遵循共同 replacement policy而不改变原字符编辑位置

#### Scenario: Wrapped RTL mixed paragraph
- **WHEN** RTL 混合段落缩小宽度、包含硬换行或开启 ellipsis
- **THEN** 每行覆盖对应逻辑范围，视觉重排在该行范围执行，所有 glyph 只出现一次，ellipsis 仍按合法 grapheme 前缀选择内容

### Requirement: Visual caret mapping
系统 SHALL 将合法 grapheme byte boundary映射为视觉 caret，允许双向 run 与软换行边界有 upstream/downstream affinity。视觉左右、行首尾、二维命中及上下移动使用实际几何；查询 MUST 不分配，过期 revision 或非法边界拒绝。

#### Scenario: Ambiguous boundary and visual navigation
- **WHEN** 同一 byte boundary 在 run 交界或软换行处对应两个视觉位置
- **THEN** affinity 能选择所需位置，左右键与指针保留该位置，等位置 stops 顺序确定且导航不在同点无限循环

#### Scenario: Multiline hit and vertical move
- **WHEN** 指针点击或从混合行上下移动
- **THEN** 系统命中合法 grapheme，按当前行视觉位置与期望 x 选取目标；Home/End 使用视觉行边缘

### Requirement: Logical editing and visual coverage
系统 SHALL 保持 value/selection/history/clipboard 的逻辑 grapheme byte 合同，将单/多行连续逻辑选择或 composition 范围显示为一到多个视觉 coverage。删除、撤销、复制及提交不按视觉次序重写 UTF-8；selected glyph view与常规显示共享一次 shape。

#### Scenario: Discontinuous selected text
- **WHEN** 连续逻辑选择跨越反方向 run 并在同一行形成不连续视觉片段
- **THEN** selection 背景与 selected text颜色只覆盖选中的片段，空隙未被染色，复制返回原逻辑子串

#### Scenario: IME and sensitive display
- **WHEN** 双向输入中有 preedit、masked Password/OTP 或迟到 composition stamp
- **THEN** IME 导航 ownership不变，native area跟随当前视觉 caret，提交仍是逻辑事务，mask不暴露原值，迟到事件拒绝

### Requirement: Reactive direction and retained lifecycle
Text/Title/Paragraph/Input 家族 SHALL 提供 typed reactive TextDirection Auto/LTR/RTL 段落配置。更新只刷新相关文字与必要布局，不重执行 slots或替换 editor/ref/scene identity；非法枚举拒绝并保留已接受状态。Core MUST 不引入 renderer/OS/SDL依赖。

#### Scenario: Live direction update and disposal
- **WHEN** 运行中改变 direction，或销毁处于 bidi/IME状态的组件
- **THEN** 相关几何和 input area正确更新，无关 Content、原值/history与长期 identity保留；销毁清理 session、scene、subscriptions与帧请求

#### Scenario: Idle and portable verification
- **WHEN** 完成一次有效更新后进入 idle
- **THEN** shaping/raster和帧提交不增长，无额外 allocation或 deadline；headless构建维持 Core依赖隔离，真实 OS/GPU证据独立记录
