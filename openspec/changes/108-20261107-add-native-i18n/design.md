# Design

## Context

动机见 proposal。已核对现有 PickerLocale、PaginationLocale、TransferLocale、UploadLocale、ImageLocale、FormLocale、DescriptionsLocale、QRCodeLocale、Form required 默认参数和 ComponentHost 的 theme-only slot 路径。Astra 研究原稿仅在 ignored `out/108-i18n-astra-analysis.md`，不把它当成已实现证据。主 spec inventory 为空；107 仍承诺 ConfigProvider/App，尚未完成。

当前集成基线为 `5a3d16da`：Theme/Select 默认栈修复已验证。待整合的 Popup/Statistic 带 StatisticLocale，Feedback 带 Notice future/motion 与 Drawer lazy/push，Display 带 Empty 动态区域/共享 Image、Card 和 Listy；Result/Skeleton/Spin 另有源码交接。108 不覆盖这些共享功能，也不把它们的 source-only 状态升级为通过。

## Goals / Non-Goals

**Goals:** 完整 zh-CN/en-US；公开应用消息与同一个 ConfigProvider；显式来源和确定性 fallback；真正动态更新并保留编辑、弹层、异步状态；与 107 服务使用共同 captured context。

**Non-Goals:** 全量 ICU/CLDR、网络词典、进程 setlocale、系统语言监听、货币/复杂复数/时区转换、HTML 模板、业务 String 自动翻译、新字体/渲染器或 OS AT 桥。新增语言必须声明实际 plural/format 支持与 fallback，不能套英语规则冒充完整支持。

## Decisions

### 1. 数据与作用域分层

`include/ryn/i18n.hpp`、`src/i18n/*` 拥有轻量 LocaleId、immutable LocalePack/LocaleCatalog、MessageKey/Args/Catalog 及纯 lookup/format。复用已有 Locale 数据，补 Common/Table/Cascader/ColorPicker 等缺口，禁止反向调用组件 host。无需新第三方依赖。

`src/runtime/config_context.*` 提供小型覆盖和共享不可变 snapshot。ComponentHost 每窗口独立默认上下文，Record/build context 的普通、透明、before-child、append/lazy 路径都携带 context；Theme 包装只替换 theme，Provider 只替换配置。`ConfigProviderProps` 与 typed content 提供 locale、formatLocale、messages/catalog、direction、size、disabled、theme；theme 复用现有 ThemeScope 堆化初始化，语言变化不 resolve_theme。禁止在每层 scope/slot 中按值保留完整 ThemeConfig 或 locale 大包。

选择共享配置作用域而非进程全局 locale：保证窗口隔离和异步回调可捕获。选择一次统一 ConfigProvider 而非另造 LocaleProvider：107/108 引用同一实现，size/disabled/direction/theme 必须有真实合同，不能仅因语言接入完成就关闭 107 task 61。

### 2. 来源和 fallback

组件来源优先级为原有 slot/具体文案、显式完整组件 Locale、最近包、明确 fallback。新增轻量来源标记或 optional Prop 区分未设置与显式 false/Auto/空串/英文；禁止通过“等于英文默认值”猜测来源。复合组件不得在未显式配置时传默认英文 aggregate 封死继承。

LocaleId 为拥有值，允许自定义；内置明示 alias，未知语言保留 requested/effective 并诊断，不把 zh-Hant/zh-TW 映射简体。包构造检查合法 UTF-8、字段、fallback 图及格式数据；最后落到完整 en-US。嵌套消息先按候选 locale 查各字典层，再进入 fallback，不读取父作用域当前语言代替候选语言。

### 3. 消息与有限格式

MessageKey 显式拥有 ID、英文 fallback、参数 schema；普通 String 不翻译。I18n handle 在 mount 捕获 scope，`text` 返回响应式绑定，`translate` 返回事件时当前快照，纯函数支持无窗口使用。raw-id 查询返回 typed MissingKey，不直接输出 key。

模板仅 `{name}`、`{{`、`}}`，参数是 String/有符号或无符号整数/Decimal 等 typed variant，插入文本不二次解析。构造验证 schema；缺失、重复、多余、错型或 malformed 拒绝。try 接口提供 typed failure，便利接口遵循既有非法配置异常习惯。诊断只记录 locale/key/category，不记录业务正文，不在测量中弹框或调用用户回调。

PluralMessage 与模板独立：非负 uint64 计数，en 的 1 为 one、其余 other，zh 为 other，other 必填。数字 helper 在 StatisticLocale 集成后复用其精确路径，保留 precision、negative zero、整数/Decimal 和显式 separator 行为。日期时间 helper 选择包中的默认格式并调用现有公历函数。formatLocale、布局方向、段落方向与语言独立，不添加无实现 timezone prop。

### 4. 更新、编辑与 Form 来源

按组件 Locale group/MessageKey 做投影，使用既有 Signal/Memo 等值抑制，不引入 Theme 风格的大型 token 注册表。相同目录内容的新指针也按语义相等处理；没有依赖的 Text 不订阅。发布前验证有效配置和后代 override；只承诺当前有效 snapshot 发布，不扩大为任意用户回调异常下全树事务。

订阅归 owner lifetime，反向 weak，handle 不持裸 State/WindowServices。set、slot、formatter/service 回调后重取 generation，以最后有效更新为准。失效 handle 明确失败，不能切到其他窗口默认值。

语言切换不设置业务 value/defaultValue/open，不重新执行无关 component/slot。普通 editor 保留 draft、selection/composition/undo/focus。dirty/composing 或有活动选区的 Number/Picker/Pagination 编辑冻结解析/格式上下文；popup 文案立即更新，提交或 blur 后再更新清洁显示。语言引发的必要局部测量与 overlay reposition 允许，静态不增加 deadline。

Form `.required()` 记录 builtin，`.required(String)` 记录 literal；`FormRule.message` 采用 optional<String>，缺省 builtin、显式 String literal、显式空仍按原规则拒绝。保持常用赋值形式并记录直接 String 读取的迁移。内部 issue 保存 builtin code/args 或 literal，外部 FormResult/getFieldError 仍投影为 String；当前显示可重投影，历史已返回结果不改。不得重新运行 validator/async 或改变 touched/dirty。

### 5. 服务依赖与 Gallery

107 App 的 ref 捕获服务 owner 与调用处活的 theme/config handles。append 支持显式 CapturedContext 与窗口/owner generation；通知和模态不能只继承 holder 根上下文。App/Modal/Drawer 的动态挂载、focus lease、future、输入域仲裁由 107 完成，108 在真实服务上验上下文；依赖未就绪时该阶段保持未完成，其他独立阶段继续。

Gallery 通过公共 MessageKey/Binding 本地化壳、导航、说明和操作文案。目标/sample/action 身份、代码/API 标识及明确 fixture 文本独立保存。切语言不能 build_page/remount 当前页；提供真实公开应用例子和覆盖清单，不用全仓禁中英文正则。

### 6. 阶段、ownership 与验证

依次实施数据/config 基础、组件/编辑/格式、真实服务上下文、公开例子/Gallery。共享 runtime、ComponentHost、WindowServices、CMake、公开聚合头和 Gallery 由同一实施链/父集成者合并；组件工作树提供精确 hunk。

平台通用逻辑使用正式 `windows-msvc-headless` Debug/Release、Ninja Multi-Config、MSVC，最多 2 compiler；不要重复同义 portable/native CPU 入口。新增 catalog/context/public_api、分组件族、service、gallery tests；保留 24 层、10k 等值零分配、Theme golden 精确回归。各族先定向，最终才做一次通用收口。

OS/GPU 验收独立：Windows 正式 native preset，hidden D3D12 RGBA/readback 与真实内部输入；Linux 通过 private headless Wayland/Vulkan。普通 Gallery 启动不是后台路径，必须使用 `--page-acceptance --page-visual` 和错误对话框抑制。截图、JSON、日志均留 out，仅精简结果入库。另一平台没有实跑不能勾选。

## Risks / Trade-offs

- 来源不明确 → 保存显式标记，覆盖默认英文与显式同文案/空串的差异测试。
- 编辑格式变化 → 冻结活动编辑上下文，提交后使用最新有效格式，并测非 ASCII/反向选区与不触发 onChange。
- Form plain-field 兼容 → 公开记录 optional 来源迁移，保留常用赋值并更新 compile fixture，不使用易错哨兵。
- 共享文件和服务依赖 → 先整合各批既有功能；服务阶段明确依赖 107 的实际实现与输入仲裁，不伪造完成。
- 词典体积和 Debug 栈 → immutable 共享存储、缩短配置求值帧，禁止扩大 stack 或在每层复制大包。

## Migration Plan

保留无 Provider 英文默认、旧 Locale 结构前缀、公开 String 业务语义与 Theme golden。分阶段提交、验证后推送；回退只涉及本阶段文件。FormRule 的直接读取方使用 optional 检查/解引用，缺省由系统投影内置消息。用户已明确授权“分析后由 Sol 实现”，规划完成后继续 apply，任务只有实际完成并验证后才勾选。
