# Spec Delta

## Purpose

定义 RynUI 当前 Ant Design 设计参考版本的可复现身份和一致性规则，使公开控件、Theme/Token、文档式 Gallery、离线生成物与新组件规划在升级后使用同一正式版，而历史验收仍能准确表明原版本。

## ADDED Requirements

### Requirement: 当前参考版本有单一可验证身份
RynUI 当前设计参考 SHALL 以 2026-09-22 核实的 Ant Design 6.6.5 正式 release 为目标，保存 release tag、完整 commit、license 和所需 source 文件 SHA256。生效中的 manifest、Token、组件参考合同、Gallery 与当前文档 MUST 指向同一版本；版本或来源不匹配时生成与验证 SHALL 失败。后续出现新 release 不得在运行或构建时静默替换本次输入。

#### Scenario: 混用旧版来源
- **WHEN** 生效中的 source manifest 指向 6.6.5，而 Token lock、组件合同或生成的 Gallery metadata 中仍引用 6.5.0 当前基线
- **THEN** 一致性检查失败并列出冲突路径，不能把混合结果标记为 6.6.5 验收通过

#### Scenario: 离线重复生成
- **WHEN** 开发者在无网络环境用已核验输入运行目录与 Token 生成器的 `--check`
- **THEN** 输出与 checked-in 结果一致，不查询浮动 upstream，也不留下 Python cache 文件

### Requirement: 升级差异逐项可追溯
系统 SHALL 记录 6.5.0 到 6.6.5 的组件分类/identity、source path、Design Token identity/default/derivation、Button/Input 状态和视觉 API 差异。实际变化 SHALL 更新生效数据与测试；无变化项 SHALL 以来源对照证明等价，不得仅做版本字符串替换。被移除或改变的公开行为 MUST 有明确兼容性处置，不能静默破坏既有 consumer。

#### Scenario: 分类或 Token 集合变化
- **WHEN** 6.6.5 的分类、组件条目数或 Token 集合不同于旧版
- **THEN** manifest、支持状态、生成输出、Theme 映射和对应 count/order/hash 测试从新来源推导，不能沿用旧版 72 项或旧 hash 作为通过条件

#### Scenario: Button/Input 来源保持等价
- **WHEN** 比对发现某个 Button/Input 视觉或交互规则在两个 release 间无差异
- **THEN** 升级记录保存两端来源和等价判断，既有公开行为及回归测试保持有效，不为了版本号改动无关实现

### Requirement: 当前资料与历史证据严格分离
面向当前使用者的 README、架构入口、Token 文档和 Gallery SHALL 说明升级后的统一基线；旧 OpenSpec change 的历史 proposal、任务、测试结果、截图与平台证据 MUST 保留原始版本和真实状态。旧版 evidence 不得直接满足新版 Windows/Linux 真实窗口验收，未完成的平台任务不能因版本升级自动勾选。

#### Scenario: 旧 Windows Gallery 证据
- **WHEN** change 008 留有 6.5.0 的 Windows 截图或未完成验收 checkbox，而当前 Gallery 已迁移到 6.6.5
- **THEN** 旧记录仍表明 6.5.0 的实际观察；新版必须另存独立身份、测试和实窗证据，旧 checkbox 不被伪改为通过

### Requirement: 后续组件沿统一基线实施
在途 Switch/Checkbox 与后续组件的当前实施计划 SHALL 使用升级后的同一 Ant Design 6.6.5 参考源及 typed Token 合同；公开 API 仍采用 `ryn`、typed Props/slots、reactive `Prop<T>`，`LayoutStyle` 仅控制外部布局。不得引入 React、CSS-in-JS、运行时网络访问或通用视觉 `Modifier`。

#### Scenario: 新旧规划冲突
- **WHEN** 后续组件规划仍将 6.5.0 的视觉或尺寸规则声明为当前基线
- **THEN** 规划一致性检查阻止实施收口，直到其当前目标更新为统一基线或明确标注为历史材料

### Requirement: 验收按平台和变化范围留证
离线来源、生成、Token/Theme、API、headless 行为与 benchmark SHALL 在一个受支持正式 preset 验证一次；真正依赖 OS、toolchain、窗口、字体、GPU/shader 或实际 display scale 的变化 SHALL 分别在 Windows 与原生 Linux Wayland 留证。任一平台通过 MUST 不替代另一平台或历史未完成验收。

#### Scenario: 仅 Windows 已复验
- **WHEN** 6.6.5 的平台通用合同和 Windows/MSVC 实窗验收通过，而 Linux Wayland 尚无新版证据
- **THEN** 平台通用与 Windows 项可独立完成，Linux 与最终跨平台收口保持未完成
