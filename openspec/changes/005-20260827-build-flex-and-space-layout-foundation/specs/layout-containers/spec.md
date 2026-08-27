## Purpose

定义 RynUI 公开 Flex 与 Space 容器、flex child 外部布局字段及其响应式测量放置合同，使应用能以 typed C++ DSL 组合可换行、可对齐且具有确定间距的 retained 组件树。

## ADDED Requirements

### Requirement: Flex 提供 typed Props 与 typed content
系统 SHALL 提供 `ryn::FlexProps`、`ryn::Flex` 和专用 typed content closure；方向、wrap、justify、align、gap 与 `LayoutStyle` MUST 使用 RynUI 自有 typed value 和 reactive `Prop<T>`，不得接受 CSS 字符串、通用 `Modifier` 或内部 layout 类型。

#### Scenario: 公开 Flex 挂载异构内容
- **WHEN** consumer 在 Flex content 中按顺序声明 Text、Button 与嵌套 Flex
- **THEN** 三个直接 child 按声明顺序挂载为稳定 retained identity，Flex 不增加可见 Primitive 或交互 target

#### Scenario: 非法公开值在编译期被拒绝
- **WHEN** consumer 尝试给 justify、align 或 gap 传入任意字符串或内部 layout enum
- **THEN** 公开类型系统不提供该入口，错误在编译期暴露

### Requirement: Flex 按主轴和交叉轴确定测量放置
Flex SHALL 默认沿 horizontal 主轴排列，并支持 horizontal/vertical、start/center/end/space-between/space-around/space-evenly justify 以及 start/center/end/stretch align；child MUST 在父约束、自己的 `LayoutStyle` 与稳定声明顺序下获得确定 bounds。

#### Scenario: Horizontal 主轴分配剩余空间
- **WHEN** 有限宽度 Flex 使用 `space-between` 放置三个未 grow 的 child
- **THEN** 首尾 child 分别贴近内容区起止边界，其余可用主轴空间等分到两个间隔

#### Scenario: Vertical 交叉轴居中
- **WHEN** vertical Flex 的内容区宽于 child 且 align 为 center
- **THEN** child 在交叉轴居中，主轴顺序和自身测量尺寸保持不变

#### Scenario: Stretch 尊重显式尺寸约束
- **WHEN** Flex align 为 stretch，某个 child 没有显式交叉轴尺寸而另一个 child 设置了固定或 min/max 交叉轴尺寸
- **THEN** 自动尺寸 child 拉伸到 line 可用交叉尺寸，显式约束 child 仍按其合法约束放置

### Requirement: Flex 支持确定的 wrap 与双轴 gap
Flex SHALL 支持不换行和按可用主轴尺寸的 greedy wrap；gap MUST 支持 Small/Middle/Large preset、非负有限 custom logical value 以及独立 main/cross 数值。换行 MUST 保持每个 child 完整，不得把单个 child 拆到两行。

#### Scenario: 窄窗口触发稳定换行
- **WHEN** horizontal Flex 的 child 总主轴尺寸超过有限内容宽度且 wrap 启用
- **THEN** child 在加入后会超限时移到下一行，相同约束和 Props 下 line break 与 bounds 可重复

#### Scenario: 双轴 gap 只出现在相邻项和相邻行之间
- **WHEN** wrapped Flex 配置不同 main gap 与 cross gap
- **THEN** 同一行相邻 child 之间使用 main gap、相邻 line 之间使用 cross gap，容器首尾不额外增加 gap

#### Scenario: 非法 custom gap 被拒绝
- **WHEN** custom gap 为负数、NaN 或无穷值
- **THEN** 挂载或响应式更新 fail-fast，上一份已提交布局保持有效

### Requirement: LayoutStyle 表达 Flex child 外部布局
公开 `LayoutStyle` SHALL 增加非负有限 grow、shrink，可选 basis、align-self 与有符号 order，并只在直接父 Flex 的布局中控制 child 的外部空间分配与放置；这些字段 MUST NOT 改变 Button/Text 的视觉 token、content 或组件 identity。

#### Scenario: Grow 按权重分配正自由空间
- **WHEN** 同一 line 的两个 child grow 分别为 1 和 2 且存在正自由空间
- **THEN** 合法 min/max 约束应用后，剩余主轴空间按 1:2 权重分配

#### Scenario: Shrink 不越过最小尺寸
- **WHEN** 单行 child 的 basis 总和超过主轴可用空间且 shrink 启用
- **THEN** deficit 按 shrink 权重收缩，任何 child 均不小于其有效最小主轴尺寸

#### Scenario: Order 只改变布局顺序
- **WHEN** 响应式 order 改变两个 child 的视觉位置
- **THEN** Flex 按 `(order, declaration order)` 稳定放置，但 content identity、paint order 与 keyboard focus order仍保持声明顺序

#### Scenario: Flex child 字段不污染非 Flex 父级
- **WHEN** 带 grow、shrink、basis、align-self 或 order 的组件挂载在非 Flex 容器
- **THEN** 字段不改变该容器的既有测量语义，也不产生视觉样式副作用

### Requirement: Space 提供受控的相邻内容间距
系统 SHALL 提供 `ryn::SpaceProps`、`ryn::Space` 和 typed content closure，首批支持 horizontal/vertical、wrap、start/center/end align、Small/Middle/Large 或自定义双轴 gap；默认 MUST 为 horizontal、no-wrap、Small gap。Space MUST 保持 direct child identity 和声明顺序，不向 consumer 暴露 item wrapper。

#### Scenario: 默认 Space 使用 Small 间距
- **WHEN** consumer 未设置 Space size 并声明三个 child
- **THEN** child 沿 horizontal 方向排列，两个相邻间隔都解析为 Default Theme 的 Small gap

#### Scenario: Vertical Space 等距排列
- **WHEN** Space 设置 vertical 与 Middle size
- **THEN** child 按声明顺序纵向排列，每对相邻 child 之间使用相同 Middle gap

#### Scenario: Space wrap 保持 item identity
- **WHEN** horizontal Space 在窗口变窄时从一行变为多行，随后恢复宽度
- **THEN** direct child 不被销毁或重挂，只有测量、放置、geometry 与 HitTest snapshot 按需更新

### Requirement: Gap preset 来自统一 Theme snapshot
Flex 与 Space 的 Small/Middle/Large gap SHALL 从同一个只读 Default Theme token snapshot 解析为 8、16、24 logical pixels；preset 变化 MUST 进入布局失效，不得直接写入组件 renderer 或公开任意视觉 override。

#### Scenario: Flex 与 Space 共享 preset 值
- **WHEN** Flex 和 Space 分别选择 Small、Middle 与 Large
- **THEN** 两个容器解析得到相同的 8、16、24 logical gap，且窗口像素密度不改变 logical layout 数值

#### Scenario: Preset 响应更新不改变结构
- **WHEN** 已挂载容器的 gap 从 Small 更新为 Large
- **THEN** child identity 与 content closure 执行次数不变，只请求受影响容器子树的 Measure/Layout/Geometry/HitTest

### Requirement: 布局响应保持 retained 生命周期与最小失效
Flex/Space Props 和 flex child `LayoutStyle` 的普通响应式更新 SHALL 在 UI owner thread 同步到已挂载容器；无结构变化时 MUST 复用 root、child、Scope、scene fragment 与 interaction identity，且稳定状态下不得持续请求帧。

#### Scenario: Justify 更新只重放放置
- **WHEN** child measurement 与容器尺寸未变化而 justify 从 start 更新为 center
- **THEN** 系统不重新执行 content closure、不重建 scene command topology，并只更新必要 placement、geometry 与 HitTest 数据

#### Scenario: Direction 或 wrap 更新重新测量目标子树
- **WHEN** direction 或 wrap 的响应值改变
- **THEN** 目标容器子树重新 Measure/Layout，未关联 sibling 不重新测量或重建

#### Scenario: 销毁容器释放完整子树
- **WHEN** Flex/Space、父 Scope 或 Host 被销毁后源 Signal 再次更新
- **THEN** 所有相关订阅和 child 资源已释放，更新不访问 stale identity 且不请求新帧

### Requirement: 平台边界保持 DPI 下的逻辑 UI 尺寸
SDL platform/renderer 边界 SHALL 启用 high-pixel-density window，并把 window coordinate、drawable pixel 与 RynUI logical coordinate 分开；layout token 和 `LogicalLength` MUST 保持 logical value，内容 viewport MUST 由 drawable pixel size 除以 display scale 得到，pointer coordinate MUST 按 pixel density 与 display scale 映射到相同 logical coordinate。字体 coverage MUST 按载入时 display scale 使用独立 raster pixel size，raster face 与 shaping face MUST 相互独立，shaping/measure 与 scene quad MUST 继续使用 logical coordinate，Glyph Atlas key MUST 区分实际 raster size；linear sampling 的 glyph quad MUST 包含透明 guard pixel，不得在 coverage 边界直接截断。

#### Scenario: Windows 150% 缩放保持设计尺寸
- **WHEN** Windows display scale 为 1.5、window coordinate 与 drawable pixel size 都是 960×720
- **THEN** RynUI 使用 640×480 logical viewport 渲染，14 logical pixel 字体使用 21px glyph raster 且仍按 14 logical pixel 参与测量与场景放置，32 logical pixel Button 按系统期望显示为正常物理尺寸，Pointer 命中与视觉 bounds 一致

#### Scenario: 高密度字形不放大逻辑排版
- **WHEN** 同一字体以 14 logical pixel 分别在 display scale 为 1.0 与 1.5 的环境中载入
- **THEN** 1.5 实例的实际 glyph coverage 使用更高 raster resolution，HarfBuzz advance、Text measure 与 Glyph Scene logical quad 不乘以 1.5，GPU 不再放大 1.0 density atlas 来满足目标显示尺寸

#### Scenario: Glyph 采样保留透明边缘
- **WHEN** 高密度 glyph coverage 的首列、末列或顶部像素包含非零值并使用 linear sampler
- **THEN** atlas UV 与 scene quad 在 coverage 四周包含透明 guard pixel，滤波在 quad 内过渡到零，字母边缘不呈现被矩形边界切掉的视觉缺口

#### Scenario: Drawable 或 display scale 变化刷新 viewport
- **WHEN** SDL 报告 window pixel size 或 display scale 变化
- **THEN** 平台边界重新查询窗口 metrics，发出新的 logical resize，目标布局重新计算但 Theme token 和公开 `LogicalLength` 数值不被改写

#### Scenario: 字体 density 在载入时显式绑定
- **WHEN** 示例在目标 display scale 下创建 Font Runtime 资源
- **THEN** 每个 font identity 记录 logical pixel size、实际 raster pixel size 与 effective raster scale；启动后跨输出的字体重新栅格化不在本 change 的完成声明中

### Requirement: 示例默认字体服从平台且保留显式配置
公开示例的默认 UI font chain SHALL 在 Windows 通过 DirectWrite system font collection 选择系统 UI 字体，在 Linux 通过 Fontconfig generic `sans-serif` 与语言匹配选择桌面默认字体；系统字体路径 MUST 由平台服务解析，不得硬编码或随 RynUI 分发。系统 SHALL 保留 typed custom font file 与 face index 配置边界，并按 custom、system、locked fallback 的优先级补足 glyph coverage，不得向公开 Component API 泄漏 DirectWrite 或 Fontconfig 类型。

#### Scenario: Windows 使用系统 UI 字体
- **WHEN** Windows 示例未配置 custom font 且系统提供标准 UI 字体
- **THEN** Latin 首选 Segoe UI 系列、简体中文首选 Microsoft YaHei UI，telemetry 记录 `font_source=system` 和实际 family chain

#### Scenario: Linux 使用桌面配置的默认字体
- **WHEN** Linux 示例未配置 custom font
- **THEN** Fontconfig 分别对 `sans-serif:lang=en` 与 `sans-serif:lang=zh-cn` 返回本机配置的 file、face index 与 family，示例使用该 chain 且不假定具体发行版字体名称

#### Scenario: 显式字体优先且保留系统回退
- **WHEN** 应用配置一个可载入但只覆盖部分文本的 custom font
- **THEN** 该 face 位于 chain 最前，缺失 glyph 才由平台系统字体或 locked fallback 补足，custom family 与最终 chain 都进入 telemetry

#### Scenario: 无效显式字体不被静默忽略
- **WHEN** custom font 文件不可读、face index 无效或 FreeType 无法载入
- **THEN** font chain 创建失败并返回包含目标路径的诊断，不改用系统字体伪装配置成功

### Requirement: 平台验收证据相互独立
系统 MUST 为 Linux 与 Windows 保存独立 Flex/Space 构建、CTest、真实窗口和截图清单；Linux 结果不得标记 Windows 项通过，Windows 结果也不得回退或替代 Linux 项。

#### Scenario: Linux 单独完成
- **WHEN** Linux GCC/Clang 自动测试和 Vulkan/SPIR-V 真实窗口验收完成而 Windows 尚未运行
- **THEN** Linux evidence 可标记 passed，Windows evidence 继续保持 pending

#### Scenario: Windows 必须使用 MSVC
- **WHEN** 执行 Windows 验收
- **THEN** 只有 `windows-msvc` 的 MSVC x64、D3D12/DXIL 构建与真实窗口结果可标记 Windows 清单，MinGW 或 Linux 结果不得代替
