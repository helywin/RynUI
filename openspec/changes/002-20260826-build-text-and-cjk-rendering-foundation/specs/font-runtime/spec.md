## Purpose

定义 RynUI 字体资源、Unicode coverage、fallback 与灰度 glyph rasterization 的可观察合同，使文本布局和渲染可以跨平台获得确定度量而不泄漏第三方字体类型。

## ADDED Requirements

### Requirement: 字体资源具有确定生命周期与错误结果
系统 SHALL 从显式字体资源创建稳定的 Font identity，并在资源不存在、格式无效、没有 Unicode charmap 或指定 pixel size 无法使用时返回包含失败阶段的错误，不得留下部分初始化对象。

#### Scenario: 有效字体资源完成初始化
- **WHEN** 调用方以有效字体资源、face index 和正 pixel size 创建字体
- **THEN** 系统返回可查询 metrics 与 Unicode coverage 的 Font identity

#### Scenario: 无效字体资源失败
- **WHEN** 字体文件不存在、内容无效或没有可用 Unicode charmap
- **THEN** 创建失败并报告对应阶段，已经取得的字体资源全部释放

### Requirement: Fallback chain 按声明顺序选择字体
系统 MUST 对每个 Unicode scalar 按 fallback chain 的声明顺序选择第一个具有非零 glyph coverage 的字体；所有字体都缺字时 MUST 使用明确的 replacement glyph 或返回可诊断的 missing-glyph 结果。

#### Scenario: Latin 与 CJK 使用不同字体
- **WHEN** 首选字体覆盖 Latin 但不覆盖目标 CJK codepoint，后备字体覆盖该 codepoint
- **THEN** Latin 选择首选字体，CJK 选择后备字体，重复查询返回相同 Font identity

#### Scenario: 所有字体都缺少 glyph
- **WHEN** fallback chain 中没有字体覆盖输入 codepoint
- **THEN** 系统使用配置的 replacement glyph，或返回带原 codepoint 的 missing-glyph 诊断，不得静默映射到随机 glyph

### Requirement: Glyph rasterization 输出规范化灰度 coverage
系统 SHALL 根据 Font identity、glyph id 和 pixel size 输出规范化的单通道 coverage bitmap、bearing、advance 与可见 bounds，并正确处理正负 pitch、空白 glyph 和零面积 bitmap。

#### Scenario: 可见 glyph 生成 coverage
- **WHEN** 调用方 rasterize 一个具有 outline 的有效 glyph
- **THEN** 输出 coverage 尺寸、row stride、bearing 和 advance 与该字体度量一致，coverage 行方向不受源 bitmap pitch 正负影响

#### Scenario: 空格不产生可见 bitmap
- **WHEN** 调用方 rasterize 一个具有 advance 但没有可见像素的空白 glyph
- **THEN** 输出保留 advance 且 coverage 尺寸为零，不把它当作错误

### Requirement: Font 与 glyph cache 遵守 owner thread
系统 MUST 记录 Font Runtime owner thread；同一 Font identity 的 coverage 查询、rasterization、cache mutation 和销毁只允许在 owner thread 执行，错误线程访问 MUST fail-fast 且不得改变 cache。

#### Scenario: Owner thread 重复请求命中 cache
- **WHEN** owner thread 以相同 Font identity、glyph id 和 pixel size 重复请求 glyph
- **THEN** 第二次请求复用相同结果，不再次 rasterize

#### Scenario: 非 owner thread 访问字体对象
- **WHEN** 非 owner thread 尝试 rasterize 或销毁 Font Runtime 拥有的字体对象
- **THEN** 操作以明确错误 fail-fast，既有字体和 cache 状态保持有效

### Requirement: 字体依赖来源显式且可重复
构建系统 SHALL 对字体引擎与验收字体继续使用显式 `BUNDLED|SYSTEM` 选择；`BUNDLED` 输入 MUST 固定不可变 source 与 SHA256，`SYSTEM` MUST 只接受调用方提供的规范 package/资源且不得隐式回退。

#### Scenario: BUNDLED 字体依赖
- **WHEN** 使用默认 dependency mode 配置工程
- **THEN** 构建只解析 lock 中记录的字体引擎和验收字体版本并校验内容

#### Scenario: SYSTEM 字体依赖缺失
- **WHEN** 选择 `SYSTEM` 但调用方没有提供兼容 package 或验收字体资源
- **THEN** configure fail-fast，并明确列出缺失输入，不切换到 `BUNDLED`
