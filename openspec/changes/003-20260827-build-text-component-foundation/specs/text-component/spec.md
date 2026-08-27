## Purpose

定义稳定的公开 `ryn::Text` 组件，使合法 UTF-8 内容通过 typed Props、Theme token、retained layout 和既有 glyph pipeline 获得可预测的双平台文字输出。

## ADDED Requirements

### Requirement: Text 使用拥有内容的 typed Props
系统 SHALL 提供公开 `TextProps` 与 `ryn::Text` 入口；content 字段 MUST 使用 `Prop<String>` 保存合法 UTF-8，允许 `u8` literal、静态 `String`、`Signal<String>` 或 `Binding<String>`，不得把借用的 `StringView` 保存到组件生命周期之外。

#### Scenario: UTF-8 literal 声明 Text
- **WHEN** consumer 使用 `ryn::Text` 和 `u8"设备监控"` 构造静态内容
- **THEN** 组件保存合法 UTF-8 内容并显示完全相同的 CJK 文本，无需 helper、宏或本地 code page 转换

#### Scenario: 响应式 String 更新
- **WHEN** `Signal<String>` 驱动的 content 从 Latin/CJK 混排值更新为另一个合法值
- **THEN** 只有目标 Text 重新 shaping、measurement 和必要 glyph 分配，兄弟 Text 不重新挂载

### Requirement: Text 视觉只来自语义 Props 与 Theme token
`TextProps` SHALL 提供 primary、secondary 与 disabled 的 typed tone 语义；字体 family、14px 常规正文尺寸、line height 和 tone color MUST 从固定的 Default Theme typography/alias token 读取，公开 Text Props 不得提供任意字体、字号、颜色、shader 或 Primitive style 覆盖。

#### Scenario: 默认正文 token
- **WHEN** Text 未显式设置 tone
- **THEN** 它使用 Default Theme 的 primary 常规正文字体、14px 尺寸和对应 line height

#### Scenario: Secondary tone 只改变 Material
- **WHEN** 已挂载 Text 的 tone 从 primary 更新为 secondary
- **THEN** 目标 Glyph instance 的 Material range 更新，content、shaping、measurement、atlas entry 和布局计数保持不变

#### Scenario: Disabled tone 使用禁用语义
- **WHEN** Text 设置 disabled tone
- **THEN** 输出使用 Default Theme 的 disabled text token，且该语义不被表示为调用方提供的任意 opacity 或颜色

### Requirement: Text measurement 参与 retained layout
Text SHALL 使用既有 shaped glyph advance、font metrics、line height 和父级 constraints 产生 Node measurement；有限 width 下只在既有合法 cluster 边界换行，放置后 Glyph instance 必须与 Node bounds、clip 和 translation 一致。

#### Scenario: 自然宽度正文
- **WHEN** Text 在无限或足够宽的约束下测量单行 Latin/CJK 内容
- **THEN** Node 宽度来自 shaped advance，高度来自 Theme line height，不按 codepoint 数量估算

#### Scenario: 有限宽度换行
- **WHEN** 父约束或 `LayoutStyle` width 小于内容自然宽度
- **THEN** Text 在合法 cluster 边界形成多行，Node 高度和 Glyph instance 位置反映全部行且 glyph 不丢失、不重复

#### Scenario: Translation 不触发 shaping
- **WHEN** 父布局只改变 Text 的最终放置位置
- **THEN** Glyph geometry/translation 更新，shape、measure 和 atlas upload 计数保持不变

### Requirement: 多个 Text 共享资源且身份稳定
同一组件 Host 中的 Text 组件 MUST 共享 Font Runtime、GlyphAtlas 与 renderer 资源，同时为每个 Text 保持 generation-checked 的稳定 state 和 Scene range；销毁一个 Text 不得破坏其他 Text 的 glyph 或 draw order。

#### Scenario: 两个 Text 复用 glyph
- **WHEN** 两个 Text 包含相同可见 glyph
- **THEN** 后挂载 Text 复用已有 glyph cache/atlas entry，不重复 rasterize 或上传该 glyph

#### Scenario: 销毁中间 Text
- **WHEN** Scene 顺序中的一个 Text 被销毁而相邻 Text 保留
- **THEN** 保留 Text 的内容、atlas entry、稳定顺序和后续更新保持正确，旧 range 不指向复用后的组件

### Requirement: Text 更新保持按需帧和最小失效
content、tone、constraint、外部 layout 与稳定状态 MUST 分别遵守声明的最小失效范围；没有组件 Dirty、atlas upload、窗口事件或显式 frame request 时 MUST 阻塞等待并停止 submit。

#### Scenario: Content 更新请求必要帧
- **WHEN** Text content 引入未缓存 glyph
- **THEN** 系统只为目标 Text 执行 shape/measure/layout、局部 atlas upload 和 instance rebuild，并请求提交下一帧

#### Scenario: 稳定 Text 保持空闲
- **WHEN** 所有 Text 内容、tone、约束和窗口状态保持不变
- **THEN** frame submission 不按刷新率增长，后续响应式更新能够重新唤醒

### Requirement: Text 公共 API 与平台输出可验证
公开 Text header MUST 不泄漏 Font、HarfBuzz、FreeType、Scene 或 SDL3 类型；同一示例 SHALL 在 Windows/MSVC/D3D12 与 Linux/GCC/Vulkan 显示相同 primary、secondary、disabled Latin/CJK 文本并正常退出。

#### Scenario: 公共 Text consumer 隔离编译
- **WHEN** consumer 只包含 `rynui.hpp` 并声明静态与响应式 Text
- **THEN** consumer 只需要 C++20 标准头和 RynUI public target 即可编译，不直接包含或链接第三方字体与平台 API

#### Scenario: 双平台真实窗口
- **WHEN** 使用正式 Windows 与 Linux presets 运行同一 Text 组件示例并触发 content、tone 和 width 更新
- **THEN** 两个平台都显示清晰的 Latin/CJK 文本，正常退出，并输出可核对的 mount、Prop update、shape、measure、layout、atlas、instance、draw、submit 与 idle 计数
