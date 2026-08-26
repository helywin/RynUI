## Context

`001-20260825-establish-rynui-foundation` 已建立 C++20、retained Node、细粒度 Reactive、约束布局、Quad scene、SDL3 GPU 与按需帧闭环，但 Scene 仍只有 Quad，公开 API 也没有统一的文本值语义。`docs/architecture.md` 已确定 UTF-8 → HarfBuzz → FreeType → GlyphAtlas → GPU 的长期方向，本 change 负责把该方向缩成可在 Windows/MSVC/D3D12 与 Linux/GCC/Vulkan 上验证的第一条文字链路。

文本同时跨越公开值类型、字体资源、塑形、布局、Scene、GPU texture 和 shader。若让 `std::string`、FreeType face、HarfBuzz buffer 或 SDL texture 穿透这些边界，后续 `Text`、`Button`、`Input` 与国际化接口将被实现细节锁定。因此本设计先固定值语义和模块协议，再实现最小 Latin/CJK 输出。

Ant Design 6.5.0 仅作为本 change 的桌面 Typography 可读性、默认字号与语义色层级参考；它不改变 RynUI 的 retained runtime，也不授权在本 change 中实现完整 Typography 组件。

## Goals / Non-Goals

### Goals

- 用轻量 `ryn::String`/`ryn::StringView` 统一公开 UTF-8 语义，并保留与 C++20 `char8_t` 标准类型的低成本互操作。
- 在不暴露第三方类型的前提下，得到确定的 font identity、fallback、shaping、measurement、rasterization 与 glyph cache 行为。
- 使用单通道 coverage atlas 和局部 texture upload，在同一 Scene 中保持 Quad/Glyph 的正确顺序、clip、颜色与 opacity。
- 使 content、font 和 constraint 更新只影响对应 Text run；color/opacity 更新只影响 Glyph instance Material；稳定状态停止 submit。
- 通过固定字体资源、单元测试、计数和双平台真实窗口证据验证 12–16px Latin/CJK 桌面文本。

### Non-Goals

- 不实现完整 `Text`/`Typography` 稳定组件 API、Theme Runtime、Design Token 编辑或 Ant Design 的 ellipsis、copyable、editable 语义。
- 不实现 Emoji/color font、variable font UI、IME、TextInput、Selection、Cursor、Clipboard、Undo/Redo 或富文本。
- 不实现完整 Unicode normalization、grapheme editing、UAX #9 双向段落、UAX #14 全量换行或语言学断词。
- 不进行系统字体枚举、fontconfig/DirectWrite/CoreText fallback，也不在应用运行期间下载字体。
- 不实现 atlas eviction/compaction；容量耗尽在本阶段是显式错误。

## Decisions

### 1. `ryn::String` 是 UTF-8 值边界，不是 Unicode 排版引擎

公开文本使用拥有内容的 `ryn::String` 和非拥有的 `ryn::StringView`。初始实现以 `std::u8string`/`std::u8string_view` 为存储基础，公开适配 C++20 `char8_t` 数据；对仍使用 `char` 的文件、网络或第三方 API，只提供名称明确的 UTF-8 byte adapter，不提供依赖系统 code page 的隐式转换。

`String` 保持“内容为合法 UTF-8”的不变量：strict 入口返回包含首个错误 byte offset 与错误类别的 typed result；lossy 入口按确定规则用 U+FFFD 修复并返回 replacement count。`StringView` 只能从有效 `String` 或经过同一验证的 UTF-8 view 创建，且不拥有生命周期。API 只承诺 `empty`、byte length、view 与显式 adapter 等值操作；不提供会把 byte index 误当字符的 `operator[]`。

为常用字面量提供无需 helper 的 C++ 写法：`ryn::String title = u8"设备监控";`。String 提供只匹配 `const char8_t (&)[N]` 的 owning 构造路径，排除结尾 null 后复制并复用 UTF-8 验证；不为普通 `char` array、`const char8_t*` 或运行时 `std::u8string_view` 提供同类隐式构造。运行时数据必须使用 strict/lossy 命名 factory，借用则从已有 String 的 `.view()` 取得，避免 literal view 与临时 String 的生命周期陷阱。该方案不导入 literals namespace、不占用后缀，也不使用宏。

C++20 解决 `char8_t` 的编码意图、`u8` literal 和标准容器互操作，但标准 string 不提供 shaping、normalization、grapheme、bidi 或 line breaking。上述能力继续由 Text 模块负责。第一版不引入庞大的 Qt 风格转换/区域设置 API，也不为了一个返回值引入通用 `Result<T, E>`；String 使用聚焦的 UTF-8 parse result，待全项目错误模型稳定后再统一。

备选方案是直接公开 `std::string` 或 `std::u8string`。前者无法表达编码合同，后者仍无法强制合法 UTF-8，且一旦作为稳定组件属性扩散便难以增加诊断与存储优化，因此不采用。另一个备选是立刻实现完整 Unicode String；这会把排版与编辑范围提前耦合到核心值类型，也不采用。

### 2. 第三方依赖与验收字体都经过集中 lock

依赖 lock 固定 FreeType 2.14.3、HarfBuzz 14.3.1、不可变 source URL、SHA256、license 标识与本地 license 文件。`BUNDLED` 以 `FetchContent` 获取 source archive，FreeType 使用其 CMake build；HarfBuzz 使用发行源码携带的 CMake target，关闭 utilities、subset、raster、vector、GPU 以及 GLib/Cairo/ICU/Graphite 等本 change 不需要的功能，只启用 FreeType bridge。FreeType 自身关闭 HarfBuzz 集成以消除构建环，再让 HarfBuzz 单向依赖 FreeType。

HarfBuzz 上游以 Meson 为主要 build system，但为这一项依赖增加第二套宿主构建工具会扩大 Windows/WSL bootstrap 与离线缓存合同。选择其随发行源码提供的 CMake target，并以 configure contract test 监测 target/option 漂移；若上游移除该入口，升级 change 必须显式选择 Meson 或维护受控 adapter，不得静默改变来源。

`SYSTEM` 只使用 `find_package` 解析调用方提供且版本匹配的 FreeType/HarfBuzz package，并验证所需 target；任一缺失立即 configure 失败。两种模式最终只向工程暴露 `RynUI::FreeType` 与 `RynUI::HarfBuzz` 内部 canonical target，使上层 CMake 不依赖包管理器 target 名称。

验收字体使用一份 Latin 字体与一份 Noto Sans CJK SC，以真实触发 fallback；两者固定 release asset、SHA256 和 SIL Open Font License 1.1。`BUNDLED` 只下载到 build tree，不向 Git 提交大字体二进制；`SYSTEM` 必须通过 cache variable 提供两个明确文件并校验可读性与 coverage，不扫描主机字体。应用运行时只加载调用方显式提供的字体 bytes/path。

### 3. Font、Text、Graphics 与 Renderer 保持单向边界

模块依赖按以下方向组织：

```text
ryn_string
    |
    v
ryn_font ------> FreeType
    |
    v
ryn_text ------> HarfBuzz
    |
    v
ryn_graphics (GlyphRun / GlyphPrimitive / GlyphInstance / dirty ranges)
    |
    v
ryn_renderer_sdl (SDL3 GPU texture / sampler / upload / draw)
```

`ryn_font` 和 `ryn_text` 可以在私有实现中包含第三方 header；`ryn_graphics`、Runtime 和 `include/ryn/` 只看到 RynUI 自有值类型。字体错误在边界处转换为 RynUI 错误阶段和诊断，不向上返回 `FT_Error`、`hb_*` 或 SDL 类型。

本 change 公开 `String` 值类型，但 Text layout、Font handle 与 Glyph 数据先保持 engine API；稳定 `TextProps`、typed slots、reactive `Prop<String>` 和 Theme/Component Token 接口由后续组件 change 批准，避免临时 renderer 需求固化成组件 API。

### 4. Font Runtime 与 cache 全部归属 UI owner thread

一个 Font Runtime 持有 FreeType library、字体 bytes、face 与 HarfBuzz font 的 RAII 生命周期。Font identity 由 slot 与 generation 组成；释放后旧 identity 不得命中新对象。字体 bytes 至少存活到相关 face 和 HarfBuzz font 销毁。

创建字体时选择 Unicode charmap、设置 pixel size、读取 ascent/descent/line gap，并把完成配置的 HarfBuzz font 视为不可变对象。同一 FreeType face 不跨线程使用；coverage 查询、shaping、rasterization、cache mutation 和销毁都检查 Runtime owner thread。后台预热与并行 rasterization不在本阶段范围内。

Glyph cache key 至少包含 Font identity generation、glyph id、pixel size 与 rasterization mode。空格等零面积 glyph 也缓存 metrics，但不分配 atlas region。

### 5. UTF-8 byte offset、fallback run 与 HarfBuzz cluster 保持可追溯

Text 输入先是合法 `StringView`。内部 decoder 产生 Unicode scalar 与 UTF-8 byte range；防御性 raw-byte/lossy adapter 先规范化为 String，并把 replacement 计数保留在诊断中。fallback chain 对每个 scalar 按声明顺序查询 coverage，再把连续相同 Font identity 的区间组成 shaping run。

每个 run 以 UTF-8 bytes 加入 HarfBuzz buffer，cluster 使用相对于整个规范化 String 的 byte offset；设置可单调追溯的 cluster level，并为 run 提供或推断 script、language 与 direction。输出转换为 RynUI 的 Font identity、glyph id、cluster、advance 与 offset；26.6 fixed-point 只在字体/塑形 adapter 内转换为统一 pixel value。

第一阶段正确处理水平 Latin、CJK 与中性标点。HarfBuzz 可以塑形单一 RTL run，但本 change 不提供完整 paragraph bidi reorder，因此混合方向段落不作为已支持能力；遇到需要 paragraph bidi 的输入必须产生能力诊断，而不是宣称视觉顺序正确。

### 6. Measurement 与基础换行只在 HarfBuzz cluster 边界操作

段落先塑形，再以 shaped advance、font ascent/descent、line height 和 width constraint 计算 line box。无限宽保持单行，显式 newline 强制分段；有限宽优先在空白后换行，并允许常用 CJK scalar 间边界。任何换行都只能发生在 HarfBuzz cluster 边界，单个 cluster 超宽时整体放入当前空行并报告 overflow。

这不是完整 UAX #14 实现。测试固定 Latin 空白、CJK 字符边界、连字 cluster、空文本、显式 newline 和超宽 cluster；其他语言的断词规则留给后续 Unicode line-break change。

### 7. GlyphAtlas 使用固定页、shelf allocator 与追加式稳定 entry

首版 atlas page 为 1024×1024 `R8_UNORM` coverage texture，entry 四周保留 1px 清零 padding。每页使用确定的 shelf allocator；当前页无法容纳时追加新页，默认最多 8 页。已分配 entry 的 page、rectangle 与 UV 在 Font Runtime 存活期间保持不变；达到上限返回包含请求尺寸与 page 使用量的容量错误，不覆盖活跃 entry。

CPU page 只保留创建和上传所需数据，并为每个新 entry 记录 dirty rectangle。重复 cache hit、空白 glyph 与纯 Material 更新不产生 texture dirty。首版不做 eviction 的代价是长时运行的 glyph 集可能到达上限；显式容量与统计使这一限制可观察，后续可在具有 generation 失效协议后增加淘汰。

### 8. Glyph instance 与 draw command 保留 Scene 顺序

`GlyphInstance` 包含 position、visible size、UV、color、opacity、clip 与 translation 等平台无关值；atlas page 留在关联 primitive/draw range 中，用于 texture binding 分段。Shader 采样单通道 coverage，并以 `coverage × color alpha × opacity` 形成与现有 Quad blend 合同一致的输出。

Scene 生成有序 draw command：Quad range 或绑定特定 atlas page 的 Glyph range。Renderer 只合并相邻且 pipeline、texture page、clip 与 Z order 兼容的 command，不跨越会改变视觉结果的边界重排。空白 glyph 推进 pen position但不生成 instance。

Glyph shader 延续单一 HLSL source，经现有 ShaderCross 路径生成 DXIL 与 SPIR-V；source、reflection/contract 和产物校验纳入现有 shader build graph。

### 9. 文本失效按 shape、layout、atlas、instance 分层

Text state 分别记录 content/font/style revision、constraint revision、shaped run、measurement、glyph instance range 与 Material revision：

- content、font chain、pixel size、weight、line height 或 width constraint 变化：标记对应 Text shape/measure/layout dirty。
- 新 shaping 结果引用未缓存 glyph：rasterize、分配 atlas、记录局部 texture dirty，再更新 instance range。
- color 或 opacity 变化：只更新该 Text 的 Glyph instance Material range。
- translation 或兼容的 clip 变化：只更新 instance geometry/clip range，不重新 shaping。
- 没有上述 dirty、atlas upload 或窗口事件：继续阻塞等待，不 submit 空帧。

这些更新由持久化 Text state 和 Binding 驱动，不重新执行无关 Component。002 的示例可以通过 engine Text node 验证该边界，但不提前发布完整组件 Props。

### 10. 计数、测试与真实窗口共同构成验收证据

诊断至少记录 UTF-8 replacement、fallback query、shape、glyph raster/cache hit、atlas page/entry/dirty region/uploaded bytes、Glyph instance range update、draw、submit 与 idle wait。计数用于验证关系，不宣称没有 benchmark 证据的绝对性能。

单元测试覆盖 String strict/lossy、不合法 UTF-8、fallback、cluster、measurement、换行、bitmap pitch、空白 glyph、atlas non-overlap/分页/容量、重复 cache hit、dirty range 与 owner-thread fail-fast。集成测试使用锁定的真实字体与真实 HarfBuzz/FreeType；fake 只用于难以构造的错误分支，不替代真实字体合同。

Windows 使用现有 MSVC + Ninja Multi-Config presets 验证 Debug/Release、D3D12/DXIL 与真实窗口；Linux/WSL 使用 GCC + Ninja Multi-Config 验证 Vulkan/SPIR-V 与同一 UTF-8 内容。窗口以 Ant Design 6.5.0 的 14px 常规正文、次级语义色与背景对比作为视觉参考，并同时打印局部更新和 idle 计数。

## Risks / Trade-offs

- **`char8_t` 生态互操作有显式转换成本**：HarfBuzz 等 C API 使用 `char*`。adapter 只按 byte view 转接并携带长度，不依赖 null terminator，也不进行隐式本地编码转换。
- **自有 String 增加一个公共类型**：它换来编码不变量与未来演进边界；严格限制首版 API，避免复制 Qt String 的范围。
- **按 scalar fallback 可能拆开复杂 grapheme**：本阶段验收聚焦 Latin/CJK；cluster 与 grapheme-aware fallback 的进一步提升需要独立 Unicode change。
- **HarfBuzz CMake 入口不是其主要构建路径**：固定版本并增加 configure/target contract test；升级时显式复核，不自动追随主分支。
- **固定字体资源增加下载与缓存体积**：二进制只进入 build/cache，Git 仅保存 lock 与 license；SYSTEM 模式可使用调用方已审计文件但必须显式配置。
- **无 eviction 会限制长时 glyph 集**：分页、上限与错误可预测；在 generation-safe eviction 设计完成前优先保证 entry 稳定。
- **基础换行不覆盖全部语言**：能力边界与诊断明确，避免把 Latin/CJK 验收外推成完整 Unicode 支持。

## Migration Plan

本 change 为 additive：现有 Quad 示例和公开 Reactive API 保持兼容。实施按 String/依赖、Font、Shaping、Atlas/Scene、GPU/Windows、Linux/最终验收的顺序分阶段提交；每一阶段必须保持已有测试通过。若新文本路径失败，可回退对应阶段提交而不更改 Quad renderer 的已验证合同。

完成后先同步 delta specs；只有在实现、严格校验与双平台证据完成且用户明确要求时才 archive。后续 `Text`/`Typography` 组件 change 将以 `ryn::String` 和本 change 的 engine Text 数据为输入，不重复定义编码与 glyph pipeline。
