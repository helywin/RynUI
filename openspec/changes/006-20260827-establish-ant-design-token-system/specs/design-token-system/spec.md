## Purpose

为 RynUI 提供一份离线、版本锁定且可机器校验的 Ant Design 6.5.0 Design Token 完整事实源，使后续组件开发、主题扩展和视觉验收不再依赖临时联网查询或个人解释。

## ADDED Requirements

### Requirement: Token 来源必须固定且可离线验证
系统 SHALL 把 Ant Design `6.5.0` tag、commit `740ad964dc2397f33e40944367b0536a7314cc32`、每个纳入文件的上游路径、内容 SHA256 与 license 信息记录在仓库内 manifest；普通 configure、build、test、文档生成和组件开发 MUST 不访问网络。

#### Scenario: 离线开发使用固定快照
- **WHEN** 开发者在无网络环境构建 RynUI 或查询任意 Design Token
- **THEN** 系统只从仓库内 manifest、typed snapshot 和中文参考文档解析结果，并能报告 Ant Design 版本、commit 与具体来源文件

#### Scenario: 来源内容与 hash 不一致
- **WHEN** manifest 声明的来源快照内容、文件 hash 或 commit identity 被修改但未执行正式升级流程
- **THEN** source integrity contract 失败，并指出不一致文件，不允许静默接受漂移值

### Requirement: 完整性必须由全量 inventory 证明
系统 MUST 枚举 Ant Design 6.5.0 官方 `SeedToken`、`MapToken`、`AliasToken` 和全部 `ComponentsConfig`/Component Token 字段；每个字段 MUST 拥有稳定 identity、所属 layer/category、上游名称、值类型、默认值或派生来源、support classification 与 source location，任何字段不得静默遗漏或重复占用 identity。

#### Scenario: 全量 coverage contract
- **WHEN** 运行 Token inventory coverage contract
- **THEN** 上游快照中发现的每个 Token 字段恰好映射到一个 manifest entry，coverage 为 100%，missing、duplicate 与 unclassified count 均为 0

#### Scenario: 尚未实现对应组件
- **WHEN** 上游 Component Token 属于 RynUI 尚未实现的业务组件
- **THEN** 该 Token 仍保留在 catalog 中并标记 component-not-yet-implemented 或其他明确 classification，而不是从 catalog 删除

### Requirement: Catalog 必须覆盖完整设计维度
Token catalog SHALL 覆盖品牌色与全部 preset palette、success/warning/error/info/link、neutral text/fill/border/surface/mask、字体与 typography scale、size/spacing/padding/margin/control height、line/border/radius、outer/inset/drop shadow、motion duration/easing/enable、opacity、z-index、breakpoint、focus/outline、disabled/loading/status、image、wireframe 和全部 Component Token；deprecated、internal 与 Web-specific 字段也 MUST 明确记录。

#### Scenario: 开发者查找视觉决策
- **WHEN** 开发者查询颜色、字体、间距、圆角、阴影、动效、层级、断点或组件状态中的任一设计维度
- **THEN** 本地 catalog 能返回规范名称、语义、默认值/派生关系、支持状态和组件消费边界，不要求再次访问 Ant Design 网站或源码

### Requirement: Upstream value 与 RynUI mapping 必须分离
系统 MUST 分别记录 upstream raw/default value、normalized typed value 和经批准的 RynUI adaptation；adaptation MUST 包含原因、影响范围和验收合同，不得改写 upstream 字段后仍声称与 Ant Design 快照相同。

#### Scenario: 桌面渲染需要适配 Web 语义
- **WHEN** CSS outline、box-shadow、font stack、breakpoint 或其他 Web 表达不能直接作为 C++ desktop runtime value 使用
- **THEN** catalog 保留原始语义，同时提供 typed RynUI mapping 与明确 adaptation note，runtime 不解析 CSS 字符串

### Requirement: 默认 Seed 基线必须稳定
Default Seed snapshot SHALL 至少固定 `colorPrimary=#1677ff`、`colorSuccess=#52c41a`、`colorWarning=#faad14`、`colorError=#ff4d4f`、`colorInfo=#1677ff`、`fontSize=14`、`lineWidth=1`、`borderRadius=6`、`sizeUnit=4`、`sizeStep=4`、`sizePopupArrow=16`、`controlHeight=32`、`zIndexBase=0`、`zIndexPopupBase=1000`、`opacityImage=1`、`motionUnit=0.1s`、`motionBase=0s` 与 `motion=true`，并使用 typed optional/semantic value 表达上游空字符串派生项。

#### Scenario: 默认 Seed snapshot parity
- **WHEN** 生成未覆盖的 Default Theme
- **THEN** 其 Seed 值与锁定 Ant Design 6.5.0 快照逐项相等，font family 通过平台 system UI font token 解析且不退化为硬编码应用字体路径

### Requirement: 本地规范必须是后续组件的唯一设计输入
仓库 SHALL 提供简体中文的人类可读 Token 规范和 machine-readable catalog，并声明稳定组件的视觉样式只能来自 Theme 与 Component Token；后续 OpenSpec change MUST 引用本地 Token identity，不得通过临时联网抓取重新选择同名值。

#### Scenario: 新组件进入规划
- **WHEN** 新组件 proposal、spec、design 或验收矩阵需要颜色、间距、尺寸、圆角、阴影、动效或状态值
- **THEN** 产物引用本地 global/component Token identity 与版本，若 catalog 缺少能力则先通过 Token upgrade/extension change 补充，不在组件内写孤立常量

### Requirement: Token 升级必须显式治理
切换 Ant Design 版本、改变上游来源或重新分类 Token SHALL 通过独立 OpenSpec change，输出 added/removed/renamed/default-changed/derivation-changed/classification-changed diff 和 RynUI compatibility impact；deprecated Token 的移除 MUST 提供迁移映射。

#### Scenario: 升级 Ant Design snapshot
- **WHEN** 项目选择新的 Ant Design tag
- **THEN** 升级流程在修改 runtime 前生成完整差异报告，严格校验无未分类字段，并保留旧 snapshot 的可追溯版本信息
