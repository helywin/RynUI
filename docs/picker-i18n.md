# Picker 原生国际化

108 / 2.2 的源码增量将 DatePicker、DateRangePicker（含日期时间）、TimePicker、TimeRangePicker 和 Calendar 的默认文案接到当前 ConfigProvider，并实现编辑格式冻结与 DatePicker 日期 key 保留。该源码交接尚未编译或运行测试，2.2 / 2.6 保持未勾选。

`placeholder(...)` 与完整 `.locale(PickerLocale)` 保存显式来源；英文及合法空文案仍优先，不通过内容与英文默认值比较来推断是否设置。未设置 Locale 的复合 Range 和内部时间选择器继续继承；显式外层 Locale 传给未另行覆盖 Locale 的时间子组件。typed footer、header、cell slot 和业务 labels/presets 保持原意。

语言包覆盖日期、时间、日期时间、年/季/月/周及 Range 的 placeholder、范围清除/确认/取消、Now/Today、Calendar 月/年和月份/星期。模式字段参照固定官方 Ant Design 6.6.5 的 en_US / zh_CN；无 Provider 保持英文，TimePicker 的默认 `Select a time`、showTime 的默认 `Select date` 与原公开行为一致，中文日期时间提示仍是明确的 `请选择日期和时间`。PickerLocale 的新字段全部追加在已有 aggregate 尾部。

UI 文案与输入格式分开：未设置完整组件 Locale 时，输入日期/月名/AM-PM 和默认格式取当前 `formatLocale`；placeholder、操作与面板星期文案取 `locale`。DatePicker 月份标题、Calendar 月选项与年份偏移使用当前格式语言。显式完整 PickerLocale 优先于两者；显式 format / inputFormats 和 showTime 的时间格式保持原合同。Minute / Millisecond / use12Hours 的缺省模式延续既有精度格式，Second 的 24 小时缺省采用包内 time_format。DateTimePicker 外层组合字符串使用自身 effective context；内部明确 Locale 保留其自身 editor 的语言优先级，不反向替换外层 Locale。

dirty/composing 或焦点内有活动选区时，DatePicker 与 TimePicker 保存原解析模式、Locale、备选输入格式、value-mode 与 precision/use12 上下文。语言变化不写业务 value/defaultValue/open，不触发 onChange；popup 文字立即更新。Week 文字按冻结周规则完成解析和归一化，再按最新业务 rules 验证；面板点击继续使用 live 规则。成功提交、取消或 blur 后清洁显示使用最新有效格式；非法/disabled 拒绝保留原草稿，会话票据防止旧提交尾部覆盖回调中新输入。公历值继续使用 CivilDate / TimeOfDay，未增加 ICU、时区或 instant 转换。

Range 的内部候选同时携带 Date mode/first-day 与 Time precision；pending、accepted 各端将值和语义成对保存，排序同时交换两者。父层仍逐次读取最新 min/max、step、disabled 函数、disabled-info 和 range predicate，并按排序后的端与完整范围复核；不把冻结语义解释为跳过业务校验。子模型保留原业务 predicate，去掉嵌套的 live 全模型校验；disabled-info 沿模型传入本次 effective mode/from。DateTime 的外层组合文本、内层 Time 及 Range 两端各自保留编辑上下文，内层候选经私有 typed 请求通道传 precision，日期可用性仍影响时间面板预览。

Range 在发布 typed 值前先同步子端语义；即使 typed 值相等，也更新语义并刷新需要的日期预览。排序/关闭产生的自身投影不作为独立外部 authority；独立 authority 替换与显式 reset 仍结束两端旧会话。单 Picker 的受控同步回显也只在本次回调票据内认领请求；DateTime 立即内层提交在消费后同步实际 pending time，保留用户配置的内层 onChange。

私有请求区分 Rejected、Consumed 与 Superseded：父 admission 拒绝不清 Range 草稿；合法端已 stage 后，即使另一端使整段提交暂不可行，也保存该端候选。controlled 同步回显只在本次请求票据内继承候选语义，不回显则恢复原 accepted；其他外部 value 替换、reset、关闭和禁用显式结束旧会话。in-flight stamp 抑制同一编辑会话的同步重复提交；predicate、Signal 和回调后的局部票据检查阻止旧操作覆盖新 authority、草稿或继续自动激活/关闭。

Range 的端点和 DateTime 内层 editor 票据同时检查文本会话与 composition 状态；回调在另一端开始 preedit 也会使旧请求 supersede。独立 DateTime authority 替换通过现有 reset hook 结束内层旧请求/草稿；正常同步回显保留本次候选语义。

DatePicker 使用 `(panel mode, raw CivilDate)` 保留有效 cell；Week 保持 42 个日身份。边界空格各有独立惰性身份，原导航/标题/星期前缀与富 footer 保留。refresh 只标记待更新 grid，在安全辅助 layout 阶段按目标 popup 的 captured theme/config 创建缺失项；新项暂存 inactive，发布后移交所有权，离 grid 的旧项完整退役。published count/mode 与 items 一起更新，Signal/renderer/rule 后校验 revision，合法 supersession 不作组件失败。按新日期顺序重排完整交互子树块，保留格内、footer 与外部 sibling 的 Tab 顺序；不为日期格增加 Tab stop。Time 新显示列沿相同安全阶段暂存发布。Calendar 追加末格 metadata 与最终发布 revision 检查，保留原发布前 factory rollback 合同。

两个新入口各注册一次：`rynui_i18n_components_pickers_tests` / `rynui.i18n_components_pickers`（12 组源码合同）与 `rynui_i18n_components_editors_tests` / `rynui.i18n_components_editors`（原 13 组及独立 Range cases TU）。源码覆盖真实编辑事件、嵌套/双窗、显式英文/空、Range/DateTime、反向选区/composition/undo/focus、独立格式语言、Week 与精确毫秒、受控拒绝、36/6/6 日期保留、全模式/公历边界、hit clip/Tab、Provider mount、重入/失败/卸载与 idle。Range cases 补 Week/min/custom、MS/最新 max/step/disabled-ms、DateTime 四入口与独立覆盖、formatLocale/mode、order/controlled、authority/新编辑/preedit 重入、等值请求、preset reset/替换/销毁和 reset/cancel/clear/close/disable（含禁用另一端）。catalog 旧入口追加新字段完整性和非法空字段回归；旧 index 身份断言改为同 mode/raw-date key 的保留与退役，Theme 检查仍使用紧邻变更前的快照。这些测试尚未编译或运行；编译、CTest、GPU、原生窗口及系统 IME 验证由父实施链记录实际结果。
