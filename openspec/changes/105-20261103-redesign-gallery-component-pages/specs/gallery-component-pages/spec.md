# Spec Delta

## Purpose

为 RynUI 原生 Gallery 提供可独立导航的组件文档页，使开发者能够离线查看真实样例、公开 C++ API 和明确的支持边界，同时保证页面切换、输入会话、草稿恢复与按需呈现之间的一致性，避免全目录挂载和虚假完成度。

## ADDED Requirements

### Requirement: 固定目录中的每个组件都有独立页面

Gallery SHALL 从锁定的 Ant Design 6.6.5 catalog 建立 7 类、73 个 canonical 组件页，以稳定 identity 路由；每次选择组件 MUST 在同一个原生窗口替换当前组件正文，而不是跳到所有组件共存的长文档锚点。总览、设计介绍和 Design Token SHALL 保留独立入口。

#### Scenario: 全目录可达
- **WHEN** 自动遍历每个 catalog identity，包括未实现和已弃用条目
- **THEN** 每个 identity 恰好解析到一个对应组件页，标题、类别、来源和状态与 catalog 一致，不漏项、不新增虚假的 canonical 条目

#### Scenario: 切换不同组件
- **WHEN** 从 Button 选择 TreeSelect
- **THEN** 正文成为 TreeSelect 页面，Button 页的样例不再参与布局、命中、场景同步或动画，窗口和全局导航保持存在

### Requirement: 导航搜索和状态筛选不改变当前页内容

Gallery SHALL 按 catalog 顺序提供分类导航、英文/中文名称与显式家族别名的搜索，以及全部六种支持状态的筛选。筛选 SHALL 只改变导航/总览结果；MUST 保持当前页、草稿和滚动位置，空结果提供清除条件入口，数量从真实数据计算。

#### Scenario: 搜索复合组件别名
- **WHEN** 用户搜索 Password 或 TextArea
- **THEN** 结果指向 canonical Input 页的对应样例分组，显示家族关系，组件总数仍为 73

#### Scenario: 当前页不匹配筛选
- **WHEN** 用户在 Button 页筛选 planned
- **THEN** 左侧只显示匹配条目并提示当前页不在筛选结果中，Button 正文和草稿保持，直到显式选择其他页

#### Scenario: 空类别结果
- **WHEN** 查询和状态组合不匹配任何条目
- **THEN** 显示零结果与清除条件入口，不自动打开无关页面、不编造候选条目

### Requirement: 文档页优先展示用途与真实样例

有原生样例的组件页 SHALL 按标题/双语名称与状态、用途、真实交互示例、C++ 与 API、支持/缺失范围、Design Token/来源提供内容；TOC SHALL 只包含当前页存在的章节。日志、构建命令、测试清单和任务 ID MUST NOT 占据页面主信息层级；证据标识符保留在可展开来源说明中。

#### Scenario: 首次打开 Button
- **WHEN** 用户进入 Button 页
- **THEN** 无需穿过全局介绍或验收表即可查看用途、声明范围状态和首张可交互示例卡，代码可展开，来源可追溯

#### Scenario: 样例尚未登记
- **WHEN** 条目已有原生能力但没有登记对应 Gallery 样例
- **THEN** 页面分别显示组件原始支持状态与“本页尚无已登记样例”，不把二者推导为同一完成状态

### Requirement: 支持程度来自声明范围而非勾选比例

Gallery MUST 使用 catalog 的 support status、summary、supported_scope、missing_scope、evidence_identifiers 和实际公开 API 表达能力；MUST NOT 用 tasks.md 勾选数、行政关闭、HTML 原型、样例数量或非原子能力条数计算实现百分比。状态统计 SHALL 标为“目录状态”，不宣称平台实测覆盖。

#### Scenario: 查看 implemented 的边界
- **WHEN** implemented 条目的 missing_scope 含 Web 排除项或某平台证据边界
- **THEN** 页面保留这些内容，说明 implemented 对应声明的原生范围，不显示“全平台通过”或“100%”

#### Scenario: 历史人工要求被关闭
- **WHEN** evidence 指向的历史任务含人工要求管理关闭
- **THEN** 页面不会把该勾选转成原生能力、自动测试通过或平台已验；缺少结构化平台结果时只提供来源与边界说明

### Requirement: 无原生实现的条目提供诚实说明

planned、web-only、deprecated、out-of-scope 条目 SHALL 保持独立页面，显示其真实状态、来源和未实现/排除原因；没有公开 API 或真实样例时 MUST 不出现仿制演示、虚假代码或可用性承诺。元数据保留范围 MUST 不标成可用的原生功能。

#### Scenario: 打开 Upload
- **WHEN** catalog 中 Upload 为 planned 且没有对应公开 API
- **THEN** 页面明确说明未提供稳定原生组件，显示 reference 元数据及缺失范围，不绘制可操作的文件上传器

#### Scenario: 零数量状态仍有定义
- **WHEN** 当前 web-only 或 out-of-scope 条目数为零
- **THEN** 筛选数量显示零；使用对应状态的自动夹具时，页面仍按该状态说明，不将其变成 planned 或 implemented

### Requirement: 复合家族复用 canonical 页面

Input/Search/Password/TextArea/OTP SHALL 归属于 canonical Input，并提供具名样例分组与各自真实 API 入口；相同页面的共享能力说明和统计只计一次。其他复合用例 SHALL 有唯一主归属，可通过关联入口到达，不能因依赖别的组件而预先挂载别页。

#### Scenario: 从 Input 切换 OTP 分组
- **WHEN** 用户选择 OTP
- **THEN** 只挂载 OTP 分组的样例，Input 的轻量草稿可恢复，先前编辑会话已退役，canonical 导航与支持状态仍属于 Input

#### Scenario: 打开关联样例
- **WHEN** Form 页指向 TreeSelect 的表单用例
- **THEN** 导航至 TreeSelect 的具名样例位置，使用其主归属登记，不隐式加载整个 Form 或全部 Gallery

### Requirement: 响应式文档布局保持全部内容可达

Gallery SHALL 在宽窗口展示导航、正文、TOC，在中窗口保留左侧导航并把 TOC 收入正文，在窄窗口使用可关闭的组件目录面板和单列正文。正文、代码与长能力文本 MUST 不被遮挡；纯说明内容不进入 Tab 序列，导航入口明确可操作。

#### Scenario: 代表宽度重排
- **WHEN** viewport 分别为 1440、960、728、480、360 logical units，并在宽窄之间切换
- **THEN** 标题、导航、全局控制与当前页内容可达，TOC 可操作，宽窄变化不重挂当前样例，滚动锚点按新 extent 夹紧

#### Scenario: 窄窗口选择组件
- **WHEN** 用户在窄窗口打开目录并选择 TreeSelect
- **THEN** 目录关闭，正文切换到 TreeSelect，焦点转移到可见的页面导航入口，背后的样例不能接收面板期间的输入

#### Scenario: 目录与正文装饰排版
- **WHEN** 显示目录入口、分类及组件条目，并聚焦正文的进入页面内容控件
- **THEN** 目录内容按层级缩进左对齐，整行仍可命中；正文内侧为焦点框保留空间，内容宽度与真实视口裁剪分别约束，不裁去控件两侧的合法焦点装饰

#### Scenario: 展开来源
- **WHEN** 展开当前组件的版本、路径及证据标识符来源
- **THEN** 各段按纵向顺序排列，长内容按当前可用宽度测量，不因共用原点互相覆盖

### Requirement: Theme 变化保留当前页状态与身份

Default、Dark、Compact 及 Dark 与 Compact 的组合 SHALL 通过既有 Theme 算法应用于 shell 和样例；正文使用可读的文字层级，Compact 主要改变控件与局部密度。Theme 更新 MUST 不重挂当前样例、不丢失草稿/选择、不激活其他页。

#### Scenario: 编辑中切换主题
- **WHEN** 用户在 Input 内编辑后切换 Dark 和 Compact
- **THEN** 编辑值、当前页、分组、现存组件身份保留，颜色和几何通过相关属性更新，导航与示例主题一致

### Requirement: 页内滚动与导航滚动独立

Gallery SHALL 分别维护目录 offset 和当前页 offset，以当前页章节/样例 identity 与局部距离恢复位置。滚轮 MUST 先允许当前页可消费的嵌套输入/弹层处理，再路由到命中的滚动区；页内 TOC 只移动当前正文，不触发组件切页。短内容和重排后的 offset MUST 夹紧。

#### Scenario: 嵌套编辑器消费滚轮
- **WHEN** TextArea 或 TreeSelect popup 消费一次滚轮事件
- **THEN** Gallery 正文和目录不滚动；事件未消费时只滚动命中的对应区域

#### Scenario: 离页后返回
- **WHEN** 用户滚至 Input 页的能力章节，访问 Button 后返回 Input
- **THEN** 恢复 Input 的章节/距离及分组，导航列表保留自己的 offset；旧页锚点代际不能作用于新页

#### Scenario: Space 样例滚动往返
- **WHEN** Space.Compact 样例滚出当前视口，正文滚回顶部后再次回到该样例
- **THEN** 原有 retained 身份与边界保持，填充分区在物理采样点恰好合成一次，按钮中部不出现裁剪裂缝或透明叠加条纹，真实视口外仍无内容泄漏

### Requirement: 页面草稿与组件运行状态分离

Gallery SHALL 在当前运行会话中保存已访问页的轻量编辑值、选择、家族分组与页内滚动信息；退出应用清除。MUST 不保存旧组件指针、Node/Interaction/Scene identity、focus/capture、IME preedit 或 popup open 状态。Password/OTP 默认离开分组或页面即清空并在示例说明中标识；历史撤销栈不保证跨卸载恢复。

#### Scenario: 普通输入草稿恢复
- **WHEN** 用户输入普通文字，切到其他组件再返回
- **THEN** 已提交的文字值恢复，编辑器以新 generation 挂载，不自动聚焦、不复活旧输入法组合或候选

#### Scenario: 敏感样例离页
- **WHEN** 用户在 Password 或 OTP 分组输入后离开
- **THEN** 该分组值被清空，返回时从初始示例状态开始，不把敏感值存到磁盘或外部服务

### Requirement: 切页可靠退役旧交互

组件页或家族分组切换 SHALL 在安全的事件分发边界执行，取消旧页面 popup、pointer capture/drag、键盘按压、focus、caret deadline 和输入会话，随后卸载旧页。旧事件、异步回调或动画 MUST 不能操作新页；离页不触发样例的提交动作，不受“拒绝关闭”演示状态阻塞。

#### Scenario: 带组合输入和弹层切页
- **WHEN** 旧页有进行中的 composition、TreeSelect popup 或 TextArea resize capture，随后接受有效页面切换
- **THEN** 已提交草稿可保存，未提交 preedit 取消，popup/capture/session/旧 deadline 全部结束；迟到文本、pointer up 与回调不改变新页

#### Scenario: 快速导航与挂载失败
- **WHEN** 连续请求多个页面，或目标样例在挂载中失败
- **THEN** 只提交有效 generation 的最终目标；失败显示可重试说明页，shell 保持可操作，半挂载子树和旧页面不继续响应输入

### Requirement: 键盘导航不劫持组件编辑行为

Gallery SHALL 支持 Tab/Shift+Tab、导航 Enter/Space 激活、目录搜索快捷键和可聚焦的页内导航；Ctrl+K 只在没有模态输入边界且不处于 composition 时聚焦目录搜索。Escape SHALL 优先交给最上层弹层，随后关闭窄目录；正文滚动快捷键 MUST 不覆盖输入控件自身的编辑或选区行为。

#### Scenario: 键盘切页
- **WHEN** 用户以键盘激活目录的 Input 入口
- **THEN** 可见的选中导航保留或承接焦点，提供进入示例的下一步入口，非交互标题与支持说明不成为一串额外 Tab 停靠点

#### Scenario: 模态示例阻止背景导航
- **WHEN** Modal 或 Drawer 已建立输入边界
- **THEN** 背景目录和全局快捷键遵守该边界；关闭后再导航，自动生命周期测试可直接验证强制退役路径，但用户操作不能穿透模态层

### Requirement: 非活动页不参与持续工作

Gallery SHALL 只为当前页及其活动家族分组建立组件、输入与场景数据；普通属性、搜索、页内滚动或主题更新 MUST 不执行无关页工厂。当前页的离屏内容沿用既有裁剪，稳定后停止无必要提交。资源上传和失败恢复 SHALL 继续遵守共同 renderer 合同。

#### Scenario: 目录增长不增加当前页样例工作
- **WHEN** 自动夹具为未访问页增加额外样例登记，并在 Button 页执行真实滚轮分发
- **THEN** 未访问工厂调用次数为零，活动样例节点/编辑器/动画数量不因这些登记增加；目录自身开销单独记录，不宣称全系统常数复杂度

#### Scenario: 多轮往返后稳定
- **WHEN** 自动往返 Button、Input、TreeSelect 和未实现页并结束输入
- **THEN** 旧页 mounted/interaction/deadline 计数归零，没有随往返次数增长的活资源泄漏；idle 无额外 frame submission，字体 atlas 合法共享缓存不误判为泄漏

### Requirement: 原生结果与设计规划严格区分

Gallery 相关验证 SHALL 区分平台通用自动测试、各 OS 原生集成和未验证边界。HTML 原型、源码检查、测试设计与历史人工要求关闭 MUST NOT 宣称为原生功能实现或平台实测；原始运行产物只保存在忽略目录，仓库仅记录简短结论。

#### Scenario: Linux 后台环境不可用
- **WHEN** Linux 自动窗口检查无法通过独立后台 Wayland 环境运行
- **THEN** 记录真实失败或环境阻塞，不回退用户桌面，不标记 Linux 通过，也不使已完成的 Windows 或平台通用结果回退
