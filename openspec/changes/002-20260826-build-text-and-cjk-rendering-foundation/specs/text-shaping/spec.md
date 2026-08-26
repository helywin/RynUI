## Purpose

定义 RynUI 对 UTF-8 文本进行字体 fallback、shaping、cluster 保留、基础换行与 measurement 的平台无关行为，为 Text 和后续交互组件提供稳定布局输入。

## ADDED Requirements

### Requirement: RynUI 提供轻量 UTF-8 String 值边界
系统 SHALL 提供拥有内容的 `ryn::String` 与非拥有的 `ryn::StringView`，其公开文本语义统一为 UTF-8；实现 MAY 使用 C++20 `char8_t`、`std::u8string` 与 `std::u8string_view`，但不得把 byte index 表述为 Unicode scalar、grapheme 或 glyph index。严格构造 MUST 在非法输入时返回带 byte offset 的 typed error，显式 lossy 构造 MUST 以 U+FFFD 修复并报告 replacement count。

#### Scenario: 从 C++20 UTF-8 文本构造
- **WHEN** 调用方以有效 `char8_t` UTF-8 数据创建 `ryn::String`
- **THEN** String 保留完全相同的 UTF-8 bytes，`StringView` 可无分配借用，并明确报告 byte length

#### Scenario: UTF-8 字面量直接构造 owning String
- **WHEN** 调用方使用 `ryn::String title = u8"设备监控"` 或把同类 `u8` literal 传给 owning String 属性
- **THEN** literal 通过只接受 `char8_t` array 的接口产生合法 `ryn::String`，不经过系统 code page、不要求导入 literals namespace、不依赖宏或自定义后缀

#### Scenario: 非 UTF-8 字面量不隐式进入 String
- **WHEN** 调用方把未带 `u8` 前缀的 narrow string literal 传给 String literal 构造路径或 String API
- **THEN** 编译期接口不提供隐式本地编码转换，调用方必须选择明确的 UTF-8 byte adapter

#### Scenario: 严格与 lossy 构造区分非法输入
- **WHEN** 调用方分别以 strict 与 lossy 入口提交相同非法 UTF-8 bytes
- **THEN** strict 入口返回首个错误 byte offset，lossy 入口返回只包含合法 UTF-8 的 String 与确定 replacement count

#### Scenario: String 不伪装完整 Unicode 算法
- **WHEN** 调用方需要 shaping、grapheme、双向段落或换行信息
- **THEN** 对应能力由 Text 模块产生，String API 不按 byte 隐式执行字符级索引或编辑

### Requirement: UTF-8 输入具有确定的错误策略
系统 SHALL 接受 `ryn::StringView` 并以其 UTF-8 byte offset 标识 cluster；从外部 raw bytes 进入 Text 边界时，遇到非法序列 MUST 先通过显式 lossy String 构造使用 U+FFFD 进行确定替换，输出不得越过规范化后的 String 边界。

#### Scenario: 有效 Latin/CJK 混排输入
- **WHEN** 输入包含有效的 Latin、标点与 CJK UTF-8 序列
- **THEN** 每个输出 cluster 都映射到有效输入 byte offset，且重复 shaping 产生相同结果

#### Scenario: 非法 UTF-8 序列
- **WHEN** 输入包含截断或非法 continuation byte
- **THEN** raw bytes 在 String 边界将非法部分映射为 U+FFFD，后续有效文本仍可 shaping，并报告替换计数；shaping 只消费规范化后的 StringView

### Requirement: Shaping 输出保留字体与 cluster 边界
系统 MUST 将文本按确定的字体 fallback 结果与 segment properties 划分 run，并为每个 glyph 输出 Font identity、glyph id、cluster、advance 和 offset；不得在 fallback run 边界丢失、重排或重复原始 cluster。

#### Scenario: Latin/CJK fallback shaping
- **WHEN** 一段文本需要首选 Latin 字体和 CJK 后备字体共同覆盖
- **THEN** 输出至少两个按文本顺序排列的 Glyph run，每个 glyph 关联正确字体且 cluster 可回溯到原 UTF-8 输入

#### Scenario: 连字保持 cluster
- **WHEN** 字体与文本产生多个字符到较少 glyph 的 shaping 结果
- **THEN** 输出保留 shaping engine 提供的 cluster 边界，measurement 和换行不得在该 cluster 内拆分

### Requirement: Text measurement 使用 shaped metrics
系统 SHALL 仅从 shaped glyph advance、offset、font ascent/descent 和配置 line height 计算 text bounds、baseline 与行框，不得用 Unicode codepoint 数量估算宽度。

#### Scenario: CJK 与 Latin 宽度不同
- **WHEN** 两段具有相同 codepoint 数量但 shaped advance 不同的文本被测量
- **THEN** 两段 measurement 分别反映其真实 advance，不返回按字符数生成的相同估算值

#### Scenario: 空文本
- **WHEN** 输入为空且配置有效 font size 与 line height
- **THEN** 输出零内容宽度和确定行高，不生成 glyph

### Requirement: 基础换行保持 cluster 完整
系统 SHALL 支持显式 newline、无限宽单行和有限宽多行 measurement；有限宽换行优先使用空白或 CJK 合法边界，单个不可拆 cluster 超宽时 MUST 整体放入一行并报告 overflow。

#### Scenario: CJK 文本在有限宽度换行
- **WHEN** CJK 文本的 shaped advance 超过有效 width constraint
- **THEN** 系统在合法 cluster 边界产生多行，所有 glyph 恰好出现一次且每行 baseline 确定

#### Scenario: 显式 newline
- **WHEN** 输入包含 newline
- **THEN** newline 强制开始下一行，即使前一行尚未达到 width constraint

### Requirement: 文本属性使用最小失效范围
系统 MUST 区分 shaping/layout 属性与纯 Material 属性：content、font chain、font size、weight、line height 或 width constraint 变化 MUST 使对应 Text run 重新 shaping/measure；color 或 opacity 变化 MUST 只更新已有 Glyph instance 的 Material 数据。

#### Scenario: 纯颜色更新
- **WHEN** 已挂载 Text 的 color Signal 改变且文字、字体与约束不变
- **THEN** glyph count、shaping count、Measure 和 Layout 计数保持不变，仅目标 Glyph instance Material range 更新

#### Scenario: 文本内容更新
- **WHEN** 已挂载 Text 的 content Signal 改变
- **THEN** 只重新 shaping 与测量该 Text run，并请求下一帧，不重新执行无关 Component

### Requirement: Text 输出保持平台无关
Text shaping 与 measurement 的公共或跨模块数据 SHALL 使用 RynUI 自有值类型，且不得暴露字体引擎、shaping engine 或 SDL3 的 handle、enum、header 或错误类型。

#### Scenario: 编译上层 Text consumer
- **WHEN** 一个只依赖 RynUI Text/Runtime 接口的 target 编译
- **THEN** 它不需要包含或链接任何平台、GPU 或第三方字体 API
