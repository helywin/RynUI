# Spec Delta

## Purpose

为业务表单、筛选和设置页面提供不依赖日期或时区的原生单时间输入。使用者可以输入严格的时分秒文本，或通过可滚动、可键盘操作的时间列面板完成选择，并保持现有主题、受控状态和保留内容合同。

## ADDED Requirements

### Requirement: 有效时间值与严格输入

系统 SHALL 提供有效的 24 小时时间值及可空选择，小时为 0–23，分秒为 0–59；支持秒精度 `HH:mm:ss` 与分钟精度 `HH:mm`。非法或未完成草稿 SHALL 不发布为值，分钟精度的新提交 SHALL 使用零秒。

#### Scenario: 文本提交与失焦恢复
- **WHEN** 用户提交 `23:59:58`，随后输入未完成或非法文本并失焦
- **THEN** 首次提交发布有效时间，后续草稿不改变值，失焦恢复最后接受值的格式化文本；preedit 不触发提交

#### Scenario: 分钟精度
- **WHEN** 用户以分钟精度输入 `09:30` 并提交
- **THEN** 发布的时间为 09:30:00，显示 `09:30`，不把格式化字符串作为公开值

### Requirement: 真实时间列与有限规则

系统 SHALL 提供真实的小时、分钟和可选秒列，支持独立滚动、指针、方向键、Home/End、列间切换及 Enter。步长 SHALL 为合法正整数；范围和禁用规则 SHALL 限制提交，全部不可选时不提交且导航有限结束。

#### Scenario: 步长与全禁用
- **WHEN** 分钟步长为 15，用户选择一个允许的小时和 30 分；随后禁用规则变为全部不可选
- **THEN** 前一选择可提交，之后确认不可提交，键盘与滚轮操作不会无限搜索或产生持续空闲帧

#### Scenario: 合法但未改变待选值的行激活
- **WHEN** 空选择的即时模式激活合法 00 行，或用户点击已选分钟行后按 Down
- **THEN** 前者 SHALL 请求有效 00:00:00；后者 SHALL 由分钟列处理导航；合法激活不依赖待选值差异，禁用行不激活

### Requirement: 确认、取消与受控 authority

系统 SHALL 支持受控/默认值与开合。默认确认模式中面板选择仅修改待提交时间，确认或 Enter 才请求值并关闭；Escape/外部关闭放弃未提交面板改动。可选即时模式 SHALL 在合法列选择时请求值并保持面板可继续选择。受控值始终以父级为准。

#### Scenario: 取消与受控拒绝
- **WHEN** 受控值为 08:00:00，用户在面板改为 09:15:00 后按 Escape，或确认后父级拒绝回写
- **THEN** 取消不发值变更；受控拒绝后公开值与显示仍为父级接受值，不把内部待提交值冒充已接受值

#### Scenario: 受控开合 authority 与强制边界
- **WHEN** 受控 open=true 的父级拒绝 Escape、OK、Cancel 或外部点击发出的关闭请求
- **THEN** 弹层 SHALL 保持打开且允许重试；只有 Modal/Spin、祖先隐藏、禁用或窗口失活等强制边界独立抑制旧弹层，边界恢复后必须有显式打开请求才能恢复

#### Scenario: 程序化新开合周期
- **WHEN** 一个已聚焦的周期关闭后，父级再次程序化打开且本周期尚未获得焦点
- **THEN** 弹层 SHALL 保持打开，不使用前一周期的 focus tracking 解释一次不存在的新失焦

### Requirement: 保留控件与输入边界

系统 SHALL 支持清空、禁用、只读输入、三尺寸、已有 Input 变体/状态、typed footer 与安全 ref。普通更新 SHALL 保持 Input、编辑会话和 footer 身份；隐藏、Modal/Spin 边界及销毁 SHALL 清理或抑制旧弹层，子 popup 和 footer 滚动控件先处理其输入。

#### Scenario: footer 与隐藏恢复
- **WHEN** footer 放置 Input、Select 或有限滚动 Table，随后隐藏并恢复 TimePicker 或切换 Modal/Spin 边界
- **THEN** footer 的正常输入与子浮层优先处理，旧弹层不会无请求复活，保留内容身份和已接受草稿，失效 ref 安全失败

#### Scenario: 同值 Form reset 与 retained 草稿
- **WHEN** String field adapter 的接受值仍为 08:00:00，而用户留下完整/未完成输入、preedit 或被受控父级拒绝的面板待选值后执行 Form reset
- **THEN** TimePicker SHALL 无值回调地重置自身 model、文本与待确认值；随后 Enter 不复活旧草稿；普通 validation/help/Theme 更新仍保留编辑草稿，过期 reset generation 不作用于新控件

#### Scenario: 关闭 footer 的焦点交付
- **WHEN** NORMAL motion 下聚焦的 footer Input 因 Escape 关闭弹层
- **THEN** 正常 deferred focus/blur 交付 SHALL 使 onBlur 恰好一次、Input 内部焦点与 text session 一致，结束隐藏 caret deadline 并恢复 selector；同步销毁或重新打开不得复用过期焦点 generation

### Requirement: Theme 与有限可见范围

系统 SHALL 提供完整的 TimePicker Theme 配置/继承/派生更新；最终字号和尺寸 SHALL 在发布前校验。实际 glyph、surface 和命中范围 SHALL 遵守分配的列/面板及祖先 clip；主题或尺寸更新不重建无关内容。

#### Scenario: 非法派生主题与裁剪
- **WHEN** 响应式主题产生负尺寸、不可渲染字号或非有限值，或有效面板被限制到窄/短视口
- **THEN** 非法主题在发布前拒绝并保留原状态；有效内容保持在实际 clip 内且不命中被裁掉的行，恢复视口后仍可选择
