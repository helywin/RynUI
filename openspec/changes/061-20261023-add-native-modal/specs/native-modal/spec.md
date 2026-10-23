# Spec Delta

## Purpose

为原生桌面业务流程提供受控模态确认和编辑对话框。在已有组件、输入会话和窗口弹层之上隔离背景交互，提供可预测的取消/确认、键盘焦点和关闭恢复，同时在反复打开时保留正文控件与用户草稿。功能实现、平台通用合同及各平台真实窗口证据分别记录。

## ADDED Requirements

### Requirement: 受控显示与取消确认

Modal SHALL 使用 typed reactive Props 和 typed 内容/标题/页脚 slots，open 默认为 false。默认页脚包含 OK/Cancel，允许隐藏或替换；closable、keyboard、mask 可响应变化。用户 OK/取消只发送请求，不自行覆盖 open；取消原因区分页脚 Cancel、关闭按钮、遮罩和 Escape。confirmLoading SHALL 阻止重复 OK 激活但保留取消路径。

#### Scenario: 父级拒绝关闭
- **WHEN** 用户激活 OK、Cancel 或可用的取消入口，父级不把 open 改为 false
- **THEN** Modal 仍显示，每次有效激活只产生一次对应请求，不伪造已关闭通知

#### Scenario: 关闭入口与加载
- **WHEN** closable/keyboard/mask.closable 关闭相应入口，或 confirmLoading 为 true
- **THEN** 被关闭的取消入口或正在加载的 OK 不产生请求，其他允许的入口仍可操作

#### Scenario: 真实关闭通知
- **WHEN** open 从实际显示变为隐藏并完成内部清理
- **THEN** afterClose 与 afterOpenChange(false) 各通知一次，初始隐藏不通知关闭；回调可再次打开或卸载而不访问过期资源

### Requirement: 保留正文与隐藏清理

Modal SHALL 一次预建正文、标题、自定义页脚与默认按钮，关闭只隐藏活动分支，保留控件身份和编辑值；隐藏内容不得命中、进入 Tab 或维持输入/弹层。初始隐藏内容的 autoFocus 不得启动输入会话。普通属性、主题、显示和滚动更新不得重跑这些 slots。

#### Scenario: 恢复编辑草稿
- **WHEN** 正文 Input 已编辑并打开子 Select，应用关闭后再次打开 Modal
- **THEN** 旧会话和弹层已经清理，编辑器、面板和草稿仍保持，内容只构建一次

#### Scenario: 回调中销毁
- **WHEN** onOk/onCancel/afterClose/焦点回调销毁 Modal 或其 owner
- **THEN** 捕获、订阅、输入、窗口层与绘制资源释放，不继续调用失效对象

### Requirement: 最上层交互隔离与焦点恢复

当一个或多个 Modal 显示时，系统 SHALL 只允许最上层 Modal 及其逻辑后代接收指针、焦点、输入和滚轮，背景仍绘制且自身 disabled 值不被改写。Tab/Shift+Tab SHALL 在可用入口间环绕，无入口时保留对话框焦点。打开时优先正文可用入口，其次默认 OK、其他可用入口，最后对话框；关闭后在原触发入口仍有效且允许时恢复焦点，可显式关闭恢复行为。

#### Scenario: 背景控制不可穿透
- **WHEN** 打开 Modal 后点击、Tab 导航、滚动背景控件或尝试引用聚焦背景 Input
- **THEN** 背景控件不激活、不编辑、不滚动，也不成为焦点；无蒙层颜色时仍遵守交互隔离

#### Scenario: 嵌套对话框
- **WHEN** 从 Modal A 打开 B，然后关闭或卸载 B
- **THEN** 仅 B 接收交互期间 A 的内容保留；B 关闭后恢复 A 的有效入口，关闭 A 后再恢复有效的外部触发入口，过期/禁用/隐藏入口不被恢复

#### Scenario: 取消指针捕获
- **WHEN** 背景拖动捕获尚未结束时打开 Modal，或捕获中的 Modal 被关闭/卸载
- **THEN** 失效捕获被取消，后续释放不触发背景或隐藏按钮

### Requirement: 子窗口层与输入优先级

Modal SHALL 支持其中 Input/TextArea/Select/Tooltip/Tabs 的既有合同。子弹层仍在对话框之上并受窗口边缘限制；边界外的弹层和输入不得保留有效交互。Escape SHALL 先交给当前编辑器组合事务，再交给当前子弹层，最后请求关闭 Modal。正文滚动不得抢走子编辑器、选择菜单或 Tabs 标签栏已消费的滚轮。

#### Scenario: 嵌套 Select 关闭
- **WHEN** Modal 内 Select 打开时先后按 Escape
- **THEN** 首次关闭 Select 并保留 Modal，随后才触发允许的 Modal Escape 请求；IME 事务优先于两者

#### Scenario: 外部弹层被隔离
- **WHEN** 背景存在受控或延时弹层时打开 Modal
- **THEN** 外部弹层被关闭或抑制，不浮在模态内容上接收交互；不得改写应用的受控值

### Requirement: 有限视口与正文滚动

Modal SHALL 脱离外部正常流，保留逻辑 owner/Theme，默认宽度520、顶部距离100，并支持水平居中及可选垂直居中。有限窗口内对话框宽高必须夹紧，正文可裁剪滚动且标题/页脚保持可见；正文输入焦点/插入点需要显露。滚动 SHALL 保持既有字形与组件，只更新平移/裁剪，允许首次显露文本实现；窗口 resize 后重新夹紧。

#### Scenario: 长内容和窄窗口
- **WHEN** 正文超过可用高度，用户滚动/Tab 导航或缩小窗口
- **THEN** 内容仍可到达、页脚可操作、控件不在裁剪外命中，子窗口层跟随锚点并保持视口内

#### Scenario: 非法尺寸
- **WHEN** width 或 Token 提供非法负数、零必需尺寸或非有限值
- **THEN** 挂载或更新原子拒绝，不留下部分节点/资源，不破坏先前有效值

### Requirement: 主题、空闲与支持边界

Modal SHALL 提供基于锁定 Ant Design6.6.5 的颜色、尺寸、字体、效果/阴影与层级 Token，支持嵌套继承/reset、component algorithm 和显式覆盖。纯颜色变化不得重新测量正文；无变化时不申请帧或持续定时器。支持目录 SHALL 明确首版范围和未覆盖能力，平台结果只按实际环境记录。

#### Scenario: 局部颜色与释放
- **WHEN** 修改局部 Modal 颜色并静置，随后销毁 owner
- **THEN** 正文不重跑、不测量；稳定后无持续帧/截止时间，销毁后无输入、窗口层、订阅或绘制泄漏

#### Scenario: 平台证据边界
- **WHEN** Windows/MSVC 的共同测试和 D3D12 窗口通过而无 Linux native 实测
- **THEN** 共同项与 Windows 可独立完成，Linux 项保持未勾选，自动逻辑输入不被描述为人工系统IME
