# Renderer 合同

035 建立共同 renderer 边界，036 分离 Quad/Glyph logical CPU scene 与 GPU 打包，037 完成 Effect packing 与 Core 的依赖隔离，038 集中收口纹理源视图、必需能力与资源输入限制；051 为 retained glyph 增加共同旋转，logical scene v3 / packed ABI v2。实现与验收状态以各 change 的 tasks 和 evidence 为准。Android、iOS、Web 与新 GPU renderer 属于后续工作。

## Logical CPU scene v3

Core 使用左上原点、x 向右/y 向下的 logical units。`QuadInstance::bounds` 为 `(x, y, width, height)`，宽高非负；translation 与 corner_radius 都是 logical 长度。`GlyphInstance::position_size` 为 logical `(x, y, width, height)`，`clip_bounds` 为 logical `(left, top, right, bottom)`，`translation_opacity` 前两项为 logical 平移。字体 density、quarter-pixel phase、bearing 与 atlas padding 已决定 glyph logical bounds，共同 packer 不再 round 或 rasterize。

GlyphTransform 的 pivot 为 logical point，angle_degrees 为绕 pivot 的顺时针角度；先旋转 glyph coverage 矩形再施加 translation，UV/coverage/material 不变。TextSceneService 的 pivot 输入相对 content origin，placement rebuild 时还原绝对 logical pivot。angle/pivot 必须有限，普通角度更新只 patch geometry，不改变 shaping、rasterization 或 atlas coverage。

CPU 类型没有 shader stride/offset 承诺，CPU bytes 不得直接作为 vertex upload。组件、TextSceneService、RetainedSurfaceService 和 QuadScene 只发布 CPU dirty ranges；GPU 资源和上传入口属于 renderer。新功能必须遵守此边界。

## Packed scene ABI v2

`renderer/common/scene_packing` 与 `rounded_effect_packing` 将保留的 logical CPU scene 转为下表的数据；三个 packed 类型均属于 `ryn::detail`，与 Core CPU 类型独立。v2 仅扩展 glyph rotation basis，Quad/Effect 保持原有字段；shader coverage/fragment reference 留在 renderer，Core 仅保留 logical reference。旧 packed ABI v1 必须被能力检查拒绝，不能继续使用 80-byte glyph stride。

| 数据 | 合同 |
| --- | --- |
| Quad | 48 bytes、16-byte alignment；rect 为 NDC `(left, top, width, negative-height)`，translation 为 NDC 向量；圆角为最短边的 0..0.5 比值 |
| Glyph | `GlyphGpuInstance` 96 bytes、16-byte alignment；rect/clip 为 NDC，UV 左上原点、范围 0..1，translation/opacity/rotation basis 存在 instance 数据中；字段 offset 0/16/32/48/64/80 |
| RoundedEffect | `RoundedEffectGpuInstance` 112 bytes、16-byte alignment；NDC rect，物理 pixel shape/clip、半径与效果参数；字段 offset 0/16/32/48/64/80/96；logical store 保留在 Core |
| 颜色 | RGBA 浮点 token 值直接传入现有 shader；RGB 使用 source-alpha 混合，alpha 使用 `one + one-minus-source-alpha`；现有路径未增加统一线性/sRGB 转换 |
| 顺序 | OrderedScene 的 Quad/Glyph/RoundedEffect 顺序必须保持；仅已有场景合同允许合并相邻兼容 draw |

RoundedEffect 的零 blur SDF 边缘使用固定一个物理像素的 smooth coverage，inset 以同一平滑 surface mask 限制 Gaussian 阴影；draw bounds 包含 AA guard，ancestor clip 仍为硬裁剪。四象限独立圆角 fill 共享同一 coverage，不对分割线 feather。logical reference 的 AA width 由调用者换算（1/display_scale），packed reference 与 HLSL 使用 material_params.y=1；blur>0 的 Gaussian 衰减保持。Quad 已按导数平滑圆角，outline 按同一物理 AA width 处理内外边界。此 coverage 修正来自共享 renderer，不能当作仅 Windows 的 MSAA 开关。

NDC x 向右、y 向上；完整视口范围 [-1, 1]。Glyph packer 绕 logical pivot 旋转矩形原点，并打包 `[cos, sin*H/W, -sin*W/H, cos]`，H/W 为 logical viewport 比例；HLSL VS 和 packed_glyph_vertex reference 用同一两条基得到顶点，确保非正方形 viewport 不改变物理角度。clip 边界、atlas UV 和 glyph padding 保持现有 shader 合同。backend 在消费边界转换 GPU API 特有的坐标、格式、传输对齐，不能让组件选择 shader 或 OS 类型。

`renderer/common/scene_metrics` 独立定义 SceneDeviceMetrics，三类 primitive 共用 physical pixel extent/display_scale，logical viewport = extent / scale；extent 必须正，scale 必须有限正，派生 viewport 也必须有限正。非法 metrics 在 begin upload 前拒绝；零尺寸/不可用 surface 留给宿主决定恢复时机。resources 复用 packed staging；首次、容量增长、metrics 改变全量重打包，普通更新只转换合并 dirty ranges，idle 不上传。resize 不改写 Quad/Glyph CPU store，不重建字体 atlas；Effect 继续按 logical viewport compact/cull。上传失败使资源 metrics 缓存失效，即使回到失败前的原 metrics 且无 CPU dirty，也完整重试。SceneResources 的提交失败还恢复所有参与数据。字体 DPI 变化所需的 shaping/raster 更新由文本服务现有机制负责，不能仅用重打包替代。

## 构建边界

`RYNUI_PLATFORM_BACKEND` 支持 SDL 与 HEADLESS；`RYNUI_RENDER_BACKENDS` 是非空的已知 renderer 列表 SDL_GPU、RECORDING。SDL 可与 SDL_GPU、RECORDING 或两者构建；HEADLESS 只支持 RECORDING。选择是编译期，P0 不提供运行时多后端选择器。当前原生 examples 需要 SDL_GPU，其他配置应关闭 examples。

`windows-msvc-headless` 构建生产 Core 与共同资源测试，完全跳过 SDL、libdecor、shadercross/DXC、shader 生成和默认系统字体服务。FreeType、HarfBuzz、utf8proc 和锁定验证字体仍是实际依赖。该 preset 验证后端隔离，不代表新操作系统支持。默认 Windows/MSVC 和既有 Linux presets 保持原生路径。

Core 的所有 source area 不 include renderer（包括相对路径）；其 link closure 不包含任何 renderer target，包括间接、条件表达式与 alias。renderer/common 可以 include/link Core 与共同 renderer 模块，但不能取得具体 backend/OS 依赖。`rynui_graphics` 不编译 GPU packing；正负配置 fixtures 与实际 compile graph 验证边界。

## 资源与上传

`SceneResources` 绑定唯一 SceneBackend owner 和设备 epoch，拥有 Quad/Glyph/Effect resources。调用 `synchronize(SceneCpuData)` 执行 begin → 共同资源同步 → commit；任何失败/异常都 cancel、恢复 CPU dirty 状态并失效附件，成功 commit 后才能 `attach`/呈现。低层同步清理的 dirty ranges 只在共同事务成功后有效；示例不得绕过共同事务。CPU OrderedScene 借用至提交完成，期间不得改变/销毁；附件 weak stamp 防止资源销毁、失败或 epoch 改变后访问旧资源。

backend 在上传方法返回成功前复制/拥有所需源 bytes；调用者之后可修改/释放借用数据。commit 仅表示接受命令，不表示 GPU 已完成。GlyphTextureUpload 为 R8 源 view：bytes、相对 bytes 的 source_offset、source_row_pitch 与目标 rectangle/page；源 offset 与目标 x/y 独立，最后一行只要求 width 有效 pixels。统一 validator 在复制前检查源/目标范围、stride 与溢出。common 直接借用 atlas page/dirty plan，不创建 backend padding/staging；SDL 在 mapped transfer 中逐行复制并填零 padding（row 256、batch offset 512），Recording 保存紧凑 owned pixels，取消/失败不提交。transfer offset、pixels_per_row、rows_per_layer 只存在于 SDL adapter。

显式 retire 在旧 device 存活时释放资源并使所有附件失效；backend epoch 已改变时，仅 abandon 旧代际 handle、容量、count 与 metrics，再从 CPU scene 新建 buffer 并全量重建，不能经新 device release。staging capacity 可复用。backend 必须比 SceneResources 活得更久。SDL 重建采用销毁资源/renderer 后构造新 renderer，未添加自动 device-loss 恢复。

Recording 实际复制 buffer/texture 数据、校验范围和类型、保持 handle tombstone、记录 draw 消费的 instance bytes。失败注入覆盖 begin/create/upload/commit/exception；reset 显式增加设备 epoch。该实现不栅格化，不作为 GPU 性能或视觉等价证据。

## 必需能力与资源输入限制

SceneBackend 必须显式提供 SceneBackendCapabilities：logical scene v3、packed ABI v2，Quad/Glyph/RoundedEffect、R8 sampling、ordered draws、partial uploads 与正的 maximum_buffer_bytes/texture width/height。缺任一必需能力或版本不匹配，SceneResources 在创建 sampler/buffer/texture 前给出原因；组件不静默忽略阴影、文字或顺序。SDL 实际查询 device 的 R8 sampling support；其他声明由当前 pipelines/上传实现提供。

资源限制是 backend 接受输入的上限，不是可用显存或硬件能力保证。SDL buffer limit 为 API uint32 上限，texture extent 只约束可表示输入；实际 GPU create 仍可能失败。共同 preflight 在 begin 前检查 Quad 的实际增长容量（共享 helper，初次精确 count、后续 max(required, old_capacity×2)）、Glyph 精确 count 与 live Effect 的保守 power-of-two 容量；epoch 改变不带入旧 Quad capacity。Effect budget 用 live count，可能高于 cull 后的可见 count。有 page 才检查 atlas extent，空 atlas config 不导致错误。超限使附件失效并保留 retry/dirty 数据，修正输入后可重试。

## 后续组件与 backend 的接入清单

- 组件：typed Props/slots、Prop<T> 与 Theme/Component Token；只发布 logical scene 和最小 dirty 范围，通过共同 SceneResources/SceneBackend 上传呈现；不得读取 GPU capability 或设置 transfer layout。
- backend：显式声明必需能力/版本与输入限制，实现源数据复制、opaque handle 的 kind/owner/epoch、begin/commit/cancel、ordered draws 与 submitted/deferred/failed；保持成功 upload 后 caller memory 可立即释放。
- 恢复：从同一 CPU scene/atlas 重建，保留组件/editor 身份；旧代际附件不可提交，不能通过新 device 释放旧 handle。
- 验证：以 Recording 检查真实 bytes、范围、顺序、失败与 epoch；每个真实 GPU/OS 另留 shader、字体、输入/DPI、窗口与生命周期证据。第二真实 backend、新平台宿主及自动 device-loss 等在各自 change 中实现。
