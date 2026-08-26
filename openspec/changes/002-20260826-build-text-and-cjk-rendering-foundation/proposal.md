## Why

`001-20260825-establish-rynui-foundation` 已证明响应状态到 Quad GPU 输出的最小闭环，但 RynUI 仍不能测量、塑形或显示真实文本，后续 `Button`、`Input` 和 Device Monitor MVP 因而没有可复用的文字基础。本 change 建立可在 Windows/D3D12 与 Linux/Vulkan 重复验证的 UTF-8/CJK 文本链路，同时保持公开组件样式边界与 Ant Design 6 一致。

## What Changes

- 增加独立 Font Runtime，通过 FreeType 管理字体 face、Unicode charmap、pixel size、glyph coverage 与度量，并以确定的 fallback chain 为缺字选择字体。
- 增加轻量 `ryn::String`/`ryn::StringView` UTF-8 值边界：C++20 `char8_t` 与 `std::u8string` 负责存储和借用语义，RynUI 负责合法性、确定替换与 byte offset 合同，并允许 `u8` literal 直接构造 owning String，无需宏或自定义后缀；不把 shaping、换行或通用 Unicode 算法塞入 String。
- 增加 UTF-8 text shaping 与 measurement，通过 HarfBuzz 输出保留 cluster、glyph id、advance、offset 和 font identity 的平台无关 `GlyphRun`。
- 增加灰度 coverage `GlyphAtlas`、`GlyphPrimitive`/instance store、局部 atlas texture upload 和 SDL3 GPU glyph pipeline，与既有 Quad 按稳定 Z order 合成。
- 增加中文、Latin/CJK 混排、fallback、重复 glyph cache、换行约束和响应式文本更新测试，并提供真实窗口文本示例与可观测计数。
- 延续 `BUNDLED|SYSTEM` 依赖合同：FreeType、HarfBuzz 与验收字体使用固定版本、source SHA256 和 license 记录；上层 API 不暴露第三方类型。
- 以 Ant Design 6.5.0 的桌面 Typography 可读性和语义色层级作为视觉参考，但本 change 不实现完整 `Typography` 组件、ellipsis/copyable/editable、Emoji/color font、IME/TextInput、Selection、复杂双向段落或完整 Theme Runtime。

## Capabilities

### New Capabilities

- `font-runtime`: 定义字体资源生命周期、Unicode coverage 查询、确定 fallback、FreeType 灰度 glyph rasterization、缓存与线程所有权。
- `text-shaping`: 定义 `ryn::String` UTF-8 值边界、HarfBuzz shaping、cluster/position 输出、受约束 measurement、基础换行与响应式文本失效边界。
- `glyph-atlas-rendering`: 定义 coverage atlas 分配与局部上传、Glyph instance/pipeline、Quad/Glyph 合成顺序、按需帧和双平台真实窗口验收。

### Modified Capabilities

无。`001` 尚未归档为 main specs，本 change 不以跨 change 路径伪装现有 capability 修改。

## Impact

- 新增 `include/ryn/string.hpp`、`src/text/`、字体资源/塑形/布局测试、Glyph shader 与 SDL renderer glyph 路径；扩展内部 Node/Dirty/Scene 数据，但不把 FreeType、HarfBuzz 或 SDL3 类型带入 `include/ryn/`。
- 扩展 `cmake/dependencies/`、第三方依赖 lock/license 文档和 Windows/Linux presets 验收，保持 `Ninja Multi-Config`、Windows/MSVC 与 Linux/GCC 基线。
- 主要风险是字体文件与 face 生命周期、fallback 后 cluster 连续性、bitmap pitch/像素格式、atlas 淘汰导致的失效、纹理局部上传对齐以及多 Primitive Z order；这些风险必须以确定性单元测试和真实窗口计数闭环验证。
- 可验证结果是同一示例在 Windows/D3D12/DXIL 与 WSL/Linux/Vulkan/SPIR-V 中清晰显示 Latin/CJK 混排文本，文本变化只重塑受影响 run、只上传新增 atlas 区域并在稳定状态停止提交帧。
