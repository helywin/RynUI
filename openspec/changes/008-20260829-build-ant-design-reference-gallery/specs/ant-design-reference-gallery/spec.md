## Purpose

定义 RynUI 离线 Ant Design Reference Gallery 的版本来源、设计介绍、完整组件分类、支持状态、交互语义与文档导航，使开发者能在真实桌面窗口中统一浏览和验收设计系统，而不把目录展示误认为已经实现的组件。

## ADDED Requirements

### Requirement: Gallery 使用可审计的固定上游快照
Gallery SHALL 声明 Ant Design `6.5.0` tag、commit `740ad964dc2397f33e40944367b0536a7314cc32`、catalog schema version 与生成来源；运行时 MUST 只读取随 RynUI 源码版本化的离线 catalog，不得通过网络抓取官网或根据 live-site 版本静默改变内容。

#### Scenario: 离线启动
- **WHEN** 机器无法访问网络且 Gallery 使用正式构建启动
- **THEN** Introduction、分类、组件清单、支持状态与来源信息完整可用，结果不依赖缓存网页

#### Scenario: 官网版本高于锁定快照
- **WHEN** Ant Design 官网显示 6.5.0 之后的版本或新增组件
- **THEN** Gallery 继续显示 6.5.0 快照，不把新条目混入 catalog，并明确提示升级需要独立的版本变更

### Requirement: Gallery 展示设计语言介绍与来源
Gallery SHALL 以简体中文摘要展示 Ant Design 面向企业级产品的设计系统定位，以及 Natural、Certain、Meaningful、Growing 四项设计价值；SHALL 提供 Guidelines/Resources、Front-end Implementation 和官方 Introduction 的来源入口。摘要 MUST 使用 RynUI 自有表述，不得复制整页原文或依赖 Ant Design 的 React、DOM、CSS-in-JS 和远程图片资源。

#### Scenario: 浏览 Introduction 区
- **WHEN** 用户进入 Gallery 顶部或选择 Introduction 导航项
- **THEN** 页面显示 RynUI 定位、锁定版本、四项设计价值、资源分组和官方来源链接，并区分“设计语言参考”与“RynUI C++ 实现”

### Requirement: Gallery 完整列出 Ant Design 6.5.0 组件分类
Gallery SHALL 按固定顺序展示 General、Layout、Navigation、Data Entry、Data Display、Feedback、Other 七个类别，并分别包含 4、7、7、18、20、11、5 个条目，总计 72 个 Ant Design 6.5.0 组件。每个条目 MUST 具有稳定 identity、英文名称、简体中文名称或摘要、类别、官方来源路径与 RynUI support status。

#### Scenario: Catalog 完整性检查
- **WHEN** 对离线 catalog 运行 category/count/identity contract
- **THEN** 七个类别和 72 个条目全部存在，名称与 identity 无重复，分类顺序和每类数量与锁定上游快照一致

#### Scenario: 未实现组件仍可见
- **WHEN** 某个 Ant Design 组件尚无 RynUI 公开 API
- **THEN** 对应条目仍出现在正确类别中并显示真实 support status，但不提供伪造的交互样例或可用性声明

### Requirement: 支持状态必须表达真实边界
每个组件条目 SHALL 使用 `implemented`、`partial`、`planned`、`web-only`、`deprecated` 或 `out-of-scope` 之一，并附带简短 scope/evidence 说明。`implemented` MUST 只用于其声明的 RynUI scope 已实现且有验收证据的条目；`partial` MUST 列出已支持与缺失的主要能力；planning、catalog metadata 或示意图 MUST NOT 作为实现证据。

#### Scenario: Button 条目呈现部分支持
- **WHEN** Gallery 展示 Button 且 RynUI 只提供当前 typed Button 子集
- **THEN** 条目标记为 `partial`，列出已支持的类型、尺寸、disabled/loading、pointer/keyboard/focus，并明确未实现的 Ant Design variant、icon、wave、完整 motion 或其他范围

#### Scenario: 状态变更需要证据
- **WHEN** 开发者把一个条目从 `planned` 或 `partial` 改为 `implemented`
- **THEN** catalog contract 要求可定位到已完成 OpenSpec capability、自动测试与对应平台验收，不接受只修改状态字符串

### Requirement: 非交互 reference content 不得复用 Button 语义
Introduction、分类标题、Token 说明、组件目录和未实现条目 SHALL 使用非交互 reference surface；这些内容 MUST 不注册 Button click、hover、pressed、focusable 或 focus-visible identity。只有明确的导航控件、状态筛选和真实组件样例可以进入交互树。

#### Scenario: Pointer 悬停目录条目
- **WHEN** pointer 移入非交互组件目录或 Token reference cell
- **THEN** 条目不出现 Button 的 primary hover border/text、不获得 pointer focus、不请求 click route，且 Button 状态样例保持不变

#### Scenario: Tab 遍历文档页面
- **WHEN** 用户连续按 `Tab` 浏览 Gallery
- **THEN** focus order 只包含导航、筛选和真实可交互样例，不停留在纯说明或未实现组件条目

### Requirement: Button 样例区分 hover 与 focus-visible
Gallery 中的 Default outlined Button hover SHALL 只把现有 1 logical pixel border 与前景切换为 `defaultHoverBorderColor`/`defaultHoverColor`；Primary 与 Danger solid hover SHALL 使用各自 hover fill 且不得增加独立蓝色 border。外部 focus outline SHALL 只在 keyboard focus-visible 时显示为 1 logical pixel 透明 gap 加 3 logical pixel hollow ring；pointer hover/focus、disabled 与非交互 reference content MUST 不显示该 ring。

#### Scenario: Default hover
- **WHEN** pointer 进入 enabled Default outlined Button 且 Button 没有 keyboard focus-visible
- **THEN** 现有 1px border 和文字使用 primary hover 色，focus outline opacity 保持为零，不出现额外外圈

#### Scenario: Solid variant hover
- **WHEN** pointer 分别进入 Primary 与 Danger solid Button
- **THEN** 两个 Button 只使用各自派生的 hover fill，透明 border 不形成空白边缘或蓝色外圈，前景保持可读

#### Scenario: Keyboard focus
- **WHEN** Button 通过 `Tab` 获得 focus-visible
- **THEN** 1px 透明 gap 与 3px hollow ring 可见；随后单纯 pointer move 不把该 ring 复制到 hover target

### Requirement: 文档内容在宽窄窗口中全部可到达
Gallery SHALL 提供类别导航、支持状态筛选与可裁剪的长文档 viewport；pointer wheel、键盘导航或等价确定操作 MUST 能访问全部 Introduction、Foundation/Token 和七类组件内容。宽窄窗口变化 SHALL reflow 内容且保留当前 section identity，不得把 72 个条目压缩为不可读矩阵或放置在不可达 viewport 外。

#### Scenario: 窄窗口浏览完整 catalog
- **WHEN** Gallery 在窄 viewport 中显示并从 Introduction 浏览到 Other 类别末尾
- **THEN** 所有 section 和 72 个组件条目均可到达、CJK/Latin 文本可读、clip 与 HitTest 一致，当前导航状态保持正确

#### Scenario: 响应式 resize
- **WHEN** 用户在同一运行中由宽窗口切换为窄窗口再恢复
- **THEN** 内容按当前 constraints 重排，stable catalog identity、支持状态、当前 section 和交互控件 identity 不被重建或丢失

### Requirement: Gallery 保持 retained 更新与离线验收
类别切换、状态筛选、Theme、viewport 与 scroll 更新 SHALL 只使受影响的 layout/geometry/material/clip/HitTest 范围失效；稳定后 MUST 停止 frame submission。平台通用 catalog、状态、Button state 与 navigation contract SHALL 只在一个受支持平台验收一次，Windows 与 Linux 的 window system、DPI、GPU/shader、system font 和真实窗口证据 MUST 分开保存。

#### Scenario: 稳定页面进入 idle
- **WHEN** Gallery 已完成初始布局且没有输入、Theme、viewport 或 navigation 更新
- **THEN** content closure 不重复执行、catalog 不重建、GPU 不持续提交帧

#### Scenario: 平台证据独立
- **WHEN** Windows/MSVC/D3D12/DXIL Gallery 验收完成而 Linux 真实窗口尚未重新运行
- **THEN** Windows 平台项可以独立完成，Linux 专属项保持原状态，平台通用 catalog contract 不要求重复执行
