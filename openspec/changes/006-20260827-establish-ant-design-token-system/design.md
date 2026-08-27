## Context

RynUI 已有 Ant Design 6.5.0 的局部 `DefaultThemeSnapshot`，但它把 Text、Button 与 layout gap 常量直接放在 component 内部，没有完整 catalog、分层算法、Theme scope、override 或 shadow primitive。当前 Quad shader 只支持填充 rounded rectangle；Button focus layer 通过把外扩后的实心 Quad 放在 Button 背后模拟 outline，`focus_ring_width=3` 与 `focus_ring_offset=1` 被合并为 4px 连续蓝带，无法表达真正的透明 offset。

本设计以锁定的 Ant Design `6.5.0` tag、commit `740ad964dc2397f33e40944367b0536a7314cc32` 为唯一上游快照。官方 Token 架构是 Seed → Map → Alias，并由 `ComponentsConfig` 添加 Component Token；Default、Dark、Compact Algorithm 可组合，Alias 中包含三档 box shadow、方向/inset shadow、focus outline、motion、breakpoint 等字段。RynUI 只映射设计与交互合同，不引入 React、DOM、CSS cascade 或 CSS-in-JS。

## Goals / Non-Goals

**Goals:**

- 让“完整 Token”成为可以由 coverage contract 证明的事实，而不是文档作者挑选的一组常用值。
- 日常开发、build、test 与验收完全离线；只有显式升级 change 才读取新的 Ant Design source tree。
- 提供 typed Seed/Map/Alias/Component Token、Default/Dark/Compact Algorithm、nested Theme 与细粒度 Theme invalidation。
- 提供可复用的 multi-layer outer/inset shadow 与 hollow focus outline GPU 能力，优先修正 Button 的连续 4px 蓝带。
- 保持现有 retained tree、按需帧、局部 GPU upload、MSVC/Ninja Multi-Config 与 SDL3 GPU 边界。

**Non-Goals:**

- 不在本 change 实现 catalog 中的全部 Ant Design 业务组件。
- 不把 CSS shorthand/string、CSS variable、selector 或浏览器 cascade 带入公开 API 或 runtime。
- 不保证 shadow 像素逐点复制某一个浏览器 rasterizer；锁定 logical geometry、layer、颜色、视觉容差和跨后端一致性。
- 不把 Theme 变成通用 `Modifier` 或任意组件 style map。

## Decisions

### 1. 完整性由 checked-in normalized catalog 和 coverage tool 共同保证

仓库增加：

```text
design-tokens/ant-design/6.5.0/
  sources.lock.yaml       # tag、commit、license、source path、SHA256
  catalog.yaml            # 每个 Seed/Map/Alias/Component Token 的 normalized record
  golden/                 # Default/Dark/Compact 与选定 override 的 resolved fixtures
docs/design-tokens.md     # 由 catalog 生成的中文规范和消费规则
tools/update_ant_design_tokens.py
```

`catalog.yaml` 的 entry 使用稳定 identity，至少包含 `upstream_name`、layer、category、value kind、raw/default/derivation、source location、support classification、RynUI mapping、invalidation domain、component owner、deprecated/internal 标记。`tools/update_ant_design_tokens.py` 只接收显式本地 Ant Design source directory，不负责下载网络内容；普通 CMake/CTest 不运行 importer，只验证已提交 catalog、source lock、golden 与生成头一致。

coverage tool 从锁定 TypeScript interface/source snapshot 提取字段集合，与 catalog 做 exact set comparison。动态 template 字段（preset palette、`${PresetColor}ShadowColor` 等）必须展开为确定 identity；unsupported、web-only 与 component-not-yet-implemented 仍算已分类，只有 missing/unclassified 才失败。

备选方案是只维护手写 Markdown。它无法证明 Component Token 和 internal/deprecated shadow 没被漏掉，也不能可靠生成 C++/测试，因此不采用。另一个方案是在每次 build 下载 npm package 并生成，违反离线和确定性要求，也不采用。

### 2. Upstream record、normalized value 与 runtime support 是三个独立层次

catalog 永远保留 upstream 名称和值，即使该字段是 CSS string、deprecated 或 RynUI 尚未支持。normalizer 把可移植语义转为 typed logical value，例如颜色、长度、duration、cubic-bezier、ShadowList 与 breakpoint；runtime 只编译 `support=runtime` 的映射。任何 adaptation 都有单独 identity/reason，不能覆盖 upstream record。

这允许未来组件直接引用完整 Component Token catalog，同时避免为了宣称“完整”而在第一版 runtime 暴露数百个尚无 consumer 的松散字符串字段。新组件落地时只能把已有 catalog entry 提升为 typed runtime mapping；若上游不存在目标语义，则通过 RynUI extension Token change 增加，不得塞入组件常量。

### 3. 公开 API 使用 typed Theme configuration 和 immutable resolved snapshot

公开基础值采用 `ryn` 命名空间并保持 literal-free 语义：

```cpp
struct Color;
struct LogicalOffset;
struct Duration;
struct CubicBezier;
struct BorderToken;
struct ShadowLayer;
struct ShadowList;

enum class ThemeAlgorithm { Default, Dark, Compact };

struct ThemeConfig;
struct ThemeProps;
void Theme(ThemeProps props, ThemeContent content);
```

`ThemeConfig` 只允许 typed Seed/Alias override、Algorithm chain、typed Component Token override 与 `inherit`。resolved `ThemeSnapshot` 是不可变值，包含 source manifest/version、algorithm chain、token values、per-token identity 与 invalidation metadata。应用未声明 `ryn::Theme` 时，Host 注入 Default snapshot；nested Theme 默认继承 parent snapshot，`inherit=false` 从 Default Seed 开始。Theme content 采用 typed slot，Props 采用 reactive `Prop<ThemeConfig>`，不增加通用视觉入口。

Map/Alias algorithm 由 RynUI C++ 确定性实现，包含 Ant Design palette/neutral alpha compositing、font/size/control/radius/motion 派生。正常 runtime 不调用 JS；`golden/` 保存由锁定 Ant Design 6.5.0 source 生成的输入输出，用逐字段 parity test 约束 C++ 实现。Component algorithm 默认关闭，显式启用时才在 component scope 重新派生，与 Ant Design 配置语义一致。

备选方案是公开 string-key dictionary。它无法在编译期拒绝拼写、CSS 值和错误单位，也使失效范围只能保守扩大，因此不采用。备选方案是只预计算三个固定主题，它不能支持品牌 Seed 和 component override，也不符合长期 Theme Runtime 目标。

### 4. Token subscriber 按 identity 和 invalidation domain 做差量传播

Theme scope 保存 immutable snapshot handle 和 generation。组件挂载时通过 typed accessor 读取 Token；accessor 向当前 reactive scope 记录 Token identity。ThemeConfig 更新先在临时 snapshot 完成验证与派生，再对旧/新 snapshot 做字段 diff，并按 catalog metadata 合并最小 dirty flags：

- color、opacity、shadow color、outline color → Paint/GPU material；
- shadow blur/spread/offset、border/radius → Geometry/Paint，只有实际改变 content bounds 的 component token 才进入 Measure/Layout；
- font family/weight/size/line-height → Text/Measure/Layout；
- spacing、control height、breakpoint → Measure/Layout/HitTest；
- motion duration/easing → Animation state，不重建 Structure；
- component availability/content slot 不属于 Token，不得由 Theme 修改。

只有订阅 changed Token 的组件/primitive 收到 dirty。Algorithm 或 snapshot identity 改变本身不是全树重建理由；切换回逐字段相等 snapshot 不产生帧。更新失败时旧 snapshot 保持可用。

备选方案是在 theme generation 改变时 invalidate 全树。它实现简单但会破坏 phased invalidation 和大型 Table/Tree 场景的性能边界，因此不采用。

### 5. Shadow 使用独立 RoundedEffect primitive 和 GPU pipeline

不扩大所有 `QuadInstance`。新增独立 `RoundedEffectInstance`，以一条 instance 表达一个 shadow layer 或 outline：logical shape rect/radius、effect kind、offset、blur、spread、outline width/offset、color/opacity、translation 与 clip identity。一个 `ShadowList` 展开为保持声明顺序的多个 retained effect instance；普通无 shadow Quad 不承担额外带宽。

outer shadow 以 rounded-rect signed distance 为基础，先应用 spread，再按 `sigma = blur / 2` 计算平滑 coverage，effect bounds 使用 `abs(offset) + max(0, spread) + 3*sigma + antialias guard` 扩张。inset shadow 在 shape 内反向计算 coverage，并服从 surface clip。outline 使用两个 rounded SDF 边界相减形成真正空心 ring：Button Default 为 1 logical px 透明 offset 后的 3 logical px ring，gap 不产生 coverage。

每层 shadow 作为所属 surface 的 retained scene child，outer layer 位于本 surface fill 之前；不同组件仍按 scene paint order 排序。ancestor clip 裁剪 effect，child 本身的 content rect 不充当隐式 clip。effect material 和 geometry 使用独立 dirty range；同类 layer 在一个 pipeline 中批量绘制。

备选方案是 CPU 离屏高斯模糊。它引入 texture cache、额外 pass 和 resize 抖动，第一阶段成本过高。备选方案是继续用多层实心 Quad；它不能表达 blur、negative spread、inset 或透明 outline offset，已被 Button 问题证伪。

### 6. Shader 从同一 HLSL 锁定源生成 DXIL 与 SPIR-V

`rounded_effect.hlsl` 使用与现有 shader toolchain 相同的锁定 SDL_shadercross/DXC/SPIRV-Cross 路径，离线生成 DXIL 与 SPIR-V，并添加 reflection/instance-layout/deployment contract。blend 继续使用 straight-color 输入与 alpha coverage，shader 输出与 pipeline blend 的 alpha 约定必须由测试共同锁定，避免多层 shadow 颜色在后端间加深。

平台通用的 SDF 数学、catalog、Algorithm、Scene order、dirty/allocation 和 headless tests 在一个受支持平台完成即可。Windows D3D12/DXIL 与 Linux Vulkan/SPIR-V 仅对各自 GPU/shader/driver/真实窗口行为建立独立清单；未运行的平台不影响已经完成的平台通用合同，也不得被另一个平台的真实 GPU 结果替代。

### 7. 主题示例是后续组件验收基准，不是临时 Demo

新增 public DSL 示例，以固定网格展示：Seed/preset/semantic palette、text hierarchy、font scale、spacing/control height、radius/border、Default/Dark/Compact、三档 elevation、Button default/primary/danger shadow、Drawer 四向 shadow、Tabs inset overflow、focus-visible/hover/active/disabled/loading。每个 cell 使用稳定 test id 和由 catalog identity 生成的 label，宽/窄窗口均可完整查看。

headless test 锁定 token snapshot、effect geometry、layer order、focus gap/ring、Theme update counters 和 idle。真实窗口 evidence 保存 display scale、GPU driver/shader format、主题状态、截图、effect/layer/upload/draw/submit counters 与退出码。后续组件 change 引用此规范和 catalog，不重新复制完整 Token 表。

## Risks / Trade-offs

- **[Ant Design TypeScript 中包含 template key 与 CSS-only value，自动提取可能漏项]** → 对 interface 字段、template expansion、source hash 和 normalized catalog 分别做 exact coverage；无法解析的值必须显式分类，不能跳过。
- **[完整 catalog 很大，公开 C++ header 可能膨胀]** → catalog metadata 不等于全部 runtime struct；公开头只暴露 runtime typed layer 和已实现 Component Token，未实现组件保留机器可读条目。
- **[C++ palette/alpha 算法与 JavaScript rounding 漂移]** → 使用锁定 golden fixtures 覆盖 Default/Dark/Compact、preset palette 和代表性 Seed override，规定颜色空间和舍入顺序。
- **[analytic shadow 与浏览器高斯结果存在像素差异]** → 锁定 logical 参数、alpha、无硬边/无裁切与跨后端容差；不宣称特定浏览器逐像素相等。
- **[多层大 blur 增加 overdraw]** → 独立 effect pipeline、tight 3-sigma bounds、clip/cull、instance batching 和 effect/draw counters；benchmark 分离常见三层与压力场景。
- **[Theme 更新被错误扩大为全树 Layout]** → Token identity subscription、impact metadata、逐字段 diff 与 Component/scene identity contract tests。
- **[focus ring 修正影响已有截图]** → 把连续 4px 蓝带列为明确 defect，保留上游 3px width/1px offset 数值，只修正透明 gap 与 hollow geometry，并更新平台证据。

## Migration Plan

1. 固定 source lock、catalog schema、全量 inventory 与中文规范，先让 completeness/hash contract 通过。
2. 实现公开 typed values、Seed/Map/Alias snapshot 与 Default/Dark/Compact golden parity，不迁移组件。
3. 接入 Theme scope、override/继承和 identity-based invalidation，通过 headless 生命周期与 allocation tests。
4. 新增 RoundedEffect Scene/GPU/shader pipeline，先验证 synthetic outer/inset/outline matrix，再迁移 Button focus 与 shadow。
5. 将 Text、Button、Flex、Space 从 `DefaultThemeSnapshot` 迁移到 Theme snapshot，删除重复常量并保持默认兼容。
6. 完成 catalog/Theme/shadow 示例、平台通用验收和受影响 GPU 平台证据。

每一步保持可独立提交。若 Theme 迁移需要回滚，可在删除旧 snapshot 前保留内部 adapter，把旧值从 Default resolved snapshot 投影到原结构；shadow pipeline 可通过 Token 中的 empty `ShadowList` 暂时关闭，但完成声明不得保留连续 4px focus fallback。
