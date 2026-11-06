# Spec Delta

## Purpose

为原生应用提供可响应、可组合且按窗口隔离的国际化能力，使组件内置文案、应用消息与有限格式规则可以统一切换语言，同时保留显式业务内容、编辑状态和组件生命周期。

## ADDED Requirements

### Requirement: 完整且可扩展的语言包
系统 SHALL 提供完整 zh-CN/en-US 组件语言包、不可变的自定义包和明确的格式支持范围，复用已有组件 Locale；无 Provider 时保持现有英文默认行为。

#### Scenario: 内置双语覆盖
- **WHEN** 应用选择 zh-CN 或 en-US
- **THEN** 所有内置可见文案使用对应完整包，已有 Locale 字段和显式覆盖能力保留，组件不得显示未解析的消息 ID

#### Scenario: 未知和非法语言
- **WHEN** 应用请求未知或非法语言 ID
- **THEN** 未知 ID 保留 requested 并报告实际 fallback/effective，非法 ID 被拒绝；系统不得把繁体中文默认为简体中文

### Requirement: 明确的 fallback 与消息来源
系统 SHALL 使用显式且无环的包 fallback；嵌套字典对每个候选语言先按最近作用域查找，最后使用声明的英文 fallback。普通 String 不被隐式翻译。

#### Scenario: 嵌套语言不串用
- **WHEN** 外层使用 zh-CN，内层使用 en-US 且内层字典缺少消息
- **THEN** 系统先查各层 en-US 消息，再沿明确的语言 fallback 查找，不直接采用父作用域当前中文

#### Scenario: 拒绝无效 fallback
- **WHEN** 应用构造存在循环、重复或未知引用的 fallback 图
- **THEN** 系统在发布前拒绝，并保留上一份有效配置；再次提交同一非法配置仍然被拒绝

### Requirement: 显式组件覆盖优先
系统 SHALL 按现有 slot/具体文案、显式完整组件 Locale、有效作用域包、fallback 的优先级解析默认内容。显式空字符串和与默认英文相同的显式值 SHALL 保持其原意。

#### Scenario: 显式英文和空值
- **WHEN** 应用明确设置 OK、No data 或空字符串后切换语言
- **THEN** 这些明确内容保持不变；未设置的对应组件默认文案随语言切换

#### Scenario: 复合组件继承
- **WHEN** Transfer、Upload、Picker 或其他复合组件挂载内部子控件
- **THEN** 未设置的子 Locale 继承作用域，显式外层 Locale 中的子 Locale 保持更高优先级

### Requirement: typed 应用消息与可验证参数
系统 SHALL 提供与普通 String 区分的消息声明、不可变词典、命名参数和响应式文本绑定；模板只支持命名占位符及转义括号，参数作为纯文本插入。

#### Scenario: 参数和 fallback
- **WHEN** 应用翻译含命名参数的消息，或当前词典缺少该消息
- **THEN** 系统支持合法参数重排和 UTF-8 内容，缺消息使用声明的 fallback 文案，不输出原始 key，也不再次解析插入文本

#### Scenario: 非法模板或参数
- **WHEN** 模板错误，或参数缺失、重复、多余、类型错误
- **THEN** 系统拒绝并提供 typed failure 或明确异常，不发布半截文字；诊断不保存业务参数正文

### Requirement: 有限且精确的格式能力
系统 SHALL 明确区分语言、格式语言、布局方向和业务日期时间；首版支持 zh-CN/en-US 非负整数计数复数、精确十进制数字和已有公历格式，不暗示完整 ICU、货币或时区能力。

#### Scenario: 计数与数字精度
- **WHEN** 应用格式化 0、1、2、uint64 大整数或 Decimal
- **THEN** 英语采用 one/other、中文采用 other，整数和 Decimal 不经过有损 double 中转，显式分隔符与 formatter 仍优先

#### Scenario: 日期与方向独立
- **WHEN** 应用独立切换格式语言或 layout direction
- **THEN** 系统复用既有日期时间解析器，不改变业务日期/时间值，也不隐式改变时区或段落 Auto 方向语义

### Requirement: 响应式且窗口隔离的配置作用域
系统 SHALL 提供唯一 ConfigProvider，逐字段继承 locale、format locale、消息目录、direction、size、disabled，并组合现有 Theme。显式 false、Auto 和完整覆盖 SHALL 与未设置区分。

#### Scenario: 嵌套和窗口隔离
- **WHEN** 两个窗口使用不同配置，或内层 Provider 只覆盖一个字段
- **THEN** 窗口互不影响，未覆盖字段保留父配置；Theme-only 与 locale-only 包装均保留其他上下文

#### Scenario: 等值更新
- **WHEN** 有效目录或配置语义相等，即使共享指针不同
- **THEN** 系统不发布无效通知、文字测量或多余帧；真正变更仅使依赖的文字与布局更新

### Requirement: 语言切换保留编辑和控件身份
系统 SHALL 在语言切换时保留组件、slot、编辑器身份、用户草稿、选区、composition、undo、焦点、选中 key、滚动和已打开弹层；只更新依赖国际化的显示。

#### Scenario: 正在编辑
- **WHEN** 用户在 Input、Number、Picker、Pagination、搜索框或富内容中编辑时切换语言
- **THEN** 系统不触发业务 onChange，不重挂载编辑器；dirty/composing 或有活动选区的会话保留原解析/格式上下文直到明确提交或结束编辑

#### Scenario: 弹层重新布局
- **WHEN** 打开的 Select、Popover、Modal 或带富单元格的容器切换语言
- **THEN** 弹层和保留子树继续存在，变化的文字可以重新测量并正确更新 anchor、clip 和 hit-test

### Requirement: 内置组件文案完整接入
系统 SHALL 接入实际具有内置文字的组件，包括确认按钮、空态、加载/重试、Picker/Calendar、分页/表格/穿梭、媒体操作及颜色选择等；业务 labels、payload、错误正文和无文字组件不被自动翻译。

#### Scenario: 硬编码路径
- **WHEN** 显示 Range 确认取消、Calendar 月年、Cascader 加载重试、Table 空态或 ColorPicker 默认操作
- **THEN** 对应语言立即生效，现有显式 Locale、renderer 和业务返回内容保留优先级

### Requirement: Form 校验消息具有明确来源
系统 SHALL 区分内置 required/rule 消息与显式 literal；当前显示和后续查询随语言更新，已返回的结果是快照，业务 validator 和异步错误保持原文。

#### Scenario: 已显示错误切换语言
- **WHEN** 已显示内置校验错误后切换语言
- **THEN** 当前错误文字更新，但不重新运行 validator，不改变 touched/dirty/validating，不取消有效异步任务或触发 values change

#### Scenario: 显式规则消息
- **WHEN** 应用通过 required(String) 或显式 rule message 设置内容
- **THEN** 内容保持 literal；缺省 rule message 使用内置来源，显式空 message 的既有非法配置规则保留

### Requirement: 服务弹层继承调用处活上下文
系统 SHALL 与 107 的 App/服务实现共享 captured context，使通知和模态服务使用调用处的当前语言、方向与主题；无 owner 的全局调用不得选择最近创建窗口。

#### Scenario: 嵌套调用服务
- **WHEN** 外层 App 下的内层 Provider 捕获服务 ref，并在事件中打开通知或对话框
- **THEN** 服务使用内层作用域；后续切换更新默认文案，显式 String 保持快照，显式消息 Binding 持续响应

#### Scenario: 销毁和跨窗口
- **WHEN** App/Provider 被销毁，或 ref 被跨窗口使用，或异步完成晚到
- **THEN** 过期访问明确失败或安全取消，future 依现有合同完成一次，不访问旧状态、不泄漏订阅和输入租约

### Requirement: 后台可用性与生命周期
系统 SHALL 使纯消息/格式能力不依赖窗口、renderer 或 SDL；作用域订阅归属实际 lifetime，重入以最后有效 generation 为准，静态本地化不产生周期调度。

#### Scenario: 公开 API 与深层作用域
- **WHEN** 应用仅通过公开头使用本地化，或在默认 MSVC 栈上挂载至少 24 层配置作用域
- **THEN** 公开应用可编译运行，语言数据通过不可变共享存储，不重现大配置栈溢出

#### Scenario: 退役后更新
- **WHEN** formatter/observer 中重入切换或销毁 owner，随后继续更新原 Signal
- **THEN** 系统不写入过期状态；释放后的 observer/deadline 不存活，一次正常帧后恢复 idle
