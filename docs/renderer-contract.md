# Renderer 合同

035 建立共同 renderer 边界，036 分离 Quad/Glyph logical CPU scene 与 GPU 打包，037 完成 Effect packing 与 Core 的依赖隔离，038 集中收口纹理源视图、必需能力与资源输入限制；051 为 retained glyph 增加共同旋转，logical scene v3 / packed ABI v2。实现与验收状态以各 change 的 tasks 和 evidence 为准。Android、iOS、Web 与新 GPU renderer 属于后续工作。

## Logical CPU scene v3

Core 使用左上原点、x 向右/y 向下的 logical units。`QuadInstance::bounds` 为 `(x, y, width, height)`，宽高非负；translation 与 corner_radius 都是 logical 长度。`GlyphInstance::position_size` 为 logical `(x, y, width, height)`，`clip_bounds` 为 logical `(left, top, right, bottom)`，`translation_opacity` 前两项为 logical 平移。字体 density、quarter-pixel phase、bearing 与 atlas padding 已决定 glyph logical bounds，共同 packer 不再 round 或 rasterize。

GlyphTransform 的 pivot 为 logical point，angle_degrees 为绕 pivot 的顺时针角度；先旋转 glyph coverage 矩形再施加 translation，UV/coverage/material 不变。TextSceneService 的 pivot 输入相对 content origin，placement rebuild 时还原绝对 logical pivot。angle/pivot 必须有限，普通角度更新只 patch geometry，不改变 shaping、rasterization 或 atlas coverage。

CPU 类型没有 shader stride/offset 承诺，CPU bytes 不得直接作为 vertex upload。组件、TextSceneService、RetainedSurfaceService 和 QuadScene 只发布 CPU dirty ranges；GPU 资源和上传入口属于 renderer。新功能必须遵守此边界。

## Effect logical paint / cap（107 A）

`graphics/effect_paint` 属于 Core。EffectGradientRamp 复制调用者 stops 并共享不可变所有权；至少两个有限、非递减位置，保留重复点和域外位置，不设固定少量 stop 上限。不排序、不执行组件专用 floor/CSS fixup。相同位置右连续，域外保持端点。EffectPaintField 包含 ramp 与有限、非退化 Linear/Conic domain；linear 为不同的起止点，conic 为 center、规范化 start radians 和 `(0, 2pi]` 顺时针 sweep。field 使用保留 translation 之前的 logical 坐标；零面积几何由 coverage 发布空区域，不能创建退化 field。材质以值比较，共有所有权身份不同但值相等的 paint 不产生 dirty。

native logical reference 在既有 Color 编码数值域内做 premultiplied RGBA 插值。可选 foreground / underlay 在同一 fragment 内先 source-over，再统一应用 coverage、material opacity 和祖先 opacity，输出 straight RGBA；已包含祖先因子的 material 不重复相乘。不引入线性/sRGB 转换或浏览器像素一致性声明。

RoundedEffectMaterial 在旧 color/opacity/visible 前缀后增加可选 paint；缺省仍为 solid。RoundedArcPath 在旧 start/sweep 前缀后增加默认 Round 的 ArcCap。旧 factory 和 Round 解析路径、角度与零 radius footprint 保留；新显式 factory 不自行套用组件 percent 补偿。Butt/Square 以圆弧 strip 和端帽线段的暴露联合外边界计算一次距离/coverage；厚 strip 穿过中心时保留负径向部分，内部接合与帽相交不重复合成。Square bounds 包含实际端帽角点与 AA guard。新显式非 Round 的零 radius 或零 sweep 为空。

## Packed scene ABI v2

`renderer/common/scene_packing` 与 `rounded_effect_packing` 将保留的 logical CPU scene 转为下表的数据；三个 packed 类型均属于 `ryn::detail`，与 Core CPU 类型独立。v2 仅扩展 glyph rotation basis，Quad/Effect 保持原有字段；shader coverage/fragment reference 留在 renderer，Core 仅保留 logical reference。旧 packed ABI v1 必须被能力检查拒绝，不能继续使用 80-byte glyph stride。

| 数据 | 合同 |
| --- | --- |
| Quad | 48 bytes、16-byte alignment；rect 为 NDC `(left, top, width, negative-height)`，translation 为 NDC 向量；圆角为最短边的 0..0.5 比值 |
| Glyph | `GlyphGpuInstance` 96 bytes、16-byte alignment；rect/clip 为 NDC，UV 左上原点、范围 0..1，translation/opacity/rotation basis 存在 instance 数据中；字段 offset 0/16/32/48/64/80 |
| RoundedEffect | `RoundedEffectGpuInstance` 112 bytes、16-byte alignment；NDC rect，物理 pixel shape/clip、半径与效果参数；字段 offset 0/16/32/48/64/80/96；logical store 保留在 Core |
| 颜色 | RGBA 浮点 token 值直接传入现有 shader；RGB 使用 source-alpha 混合，alpha 使用 `one + one-minus-source-alpha`；现有路径未增加统一线性/sRGB 转换 |
| 顺序 | OrderedScene 的 Quad/Glyph/RoundedEffect 顺序必须保持；仅已有场景合同允许合并相邻兼容 draw |

RoundedEffect 的零 blur SDF 边缘使用固定一个物理像素的 smooth coverage，inset 以同一平滑 surface mask 限制 Gaussian 阴影；draw bounds 包含 AA guard，ancestor clip 仍为硬裁剪。四象限独立圆角 fill 共享同一 coverage，不对分割线 feather。logical reference 的 AA width 由调用者换算（1/display_scale），packed reference 与 HLSL 使用 material_params.y=1；blur>0 的 Gaussian 衰减保持。Quad 使用同一固定一物理像素的 smooth coverage；vertex stage 读取实际 render target 物理尺寸的 16-byte uniform，在原 shape 外提交一物理像素 guard 并外延局部坐标，保留原 shape 尺寸、圆角和 48-byte instance ABI。普通 Scene、Quad 专用 renderer 与截图导出共用该规则；零面积 Quad 不扩成可见区域，viewport/真实祖先仍硬裁剪。outline 按同一物理 AA width 处理内外边界。此 coverage 修正来自共享 renderer，不能当作仅 Windows 的 MSAA 开关。

NDC x 向右、y 向上；完整视口范围 [-1, 1]。Glyph packer 绕 logical pivot 旋转矩形原点，并打包 `[cos, sin*H/W, -sin*W/H, cos]`，H/W 为 logical viewport 比例；HLSL VS 和 packed_glyph_vertex reference 用同一两条基得到顶点，确保非正方形 viewport 不改变物理角度。clip 边界、atlas UV 和 glyph padding 保持现有 shader 合同。backend 在消费边界转换 GPU API 特有的坐标、格式、传输对齐，不能让组件选择 shader 或 OS 类型。

RoundedEffect 的内部 path kind 3/4/5 分别表示有限圆弧、圆角虚线和 Tooltip 联合填充；原 0/1/2 的阴影/outline 编码不变，实例仍为 112 bytes。圆弧以圆形 centerline、顺时针起止角和 stroke 表达，零 sweep 为空，端点使用圆端帽；rail 与进度各只合成一次。虚线使用完整 rounded-rect 周长参数，整周均匀拟合 dash/gap 比例，角点不重启 phase，Compact 的四角 mask 保留。Tooltip 用主体外边界替换内部 base 段并接到两条三角斜边，单次 SDF/alpha 消除 glyph 像素相位白缝和透明重叠加深。

新 path 的 CPU geometry 为 typed variant，验证类型、有限数值、角度范围、描边及箭头 base 必须落在直边内；CPU 类型无 packed layout 承诺。packed `shadow_params.yzw` 在 kind 3 为不随 DPI 缩放的 start/sweep radians（末项为零），kind 4 为物理 fitted dash/gap/phase，kind 5 为沿边 center/half-width/length；`material_params.z` 为 kind 4 的四角 bitmask 或 kind 5 的方向。draw bounds 包含整个 path 和 AA guard，ancestor/viewport clip 仍为硬边界。新 path 使用共同资源上传、reference 和 HLSL，不增加组件私有上传或 backend 分支。

`renderer/common/scene_metrics` 独立定义 SceneDeviceMetrics，三类 primitive 共用 physical pixel extent/display_scale，logical viewport = extent / scale；extent 必须正，scale 必须有限正，派生 viewport 也必须有限正。非法 metrics 在 begin upload 前拒绝；零尺寸/不可用 surface 留给宿主决定恢复时机。resources 复用 packed staging；首次、容量增长、metrics 改变全量重打包，普通更新只转换合并 dirty ranges，idle 不上传。resize 不改写 Quad/Glyph CPU store，不重建字体 atlas；Effect 继续按 logical viewport compact/cull。上传失败使资源 metrics 缓存失效，即使回到失败前的原 metrics 且无 CPU dirty，也完整重试。SceneResources 的提交失败还恢复所有参与数据。字体 DPI 变化所需的 shaping/raster 更新由文本服务现有机制负责，不能仅用重打包替代。

RoundedEffect 的物理 draw bounds 向外覆盖整像素边界，防止硬件顶点子像素量化先于 PS 裁剪丢掉合法采样；shape、半径及 AA 宽度不取整。共享 clip 端点先在 logical 域求和，再统一缩放；PS 与 packed reference 使用左/上包含、右/下排除的硬边界，分区填充在每个采样点只合成一次，既不留缝也不叠加透明度。draw domain 的保守外延不扩大实际 ancestor clip。

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

### Effect paint / cap preflight（107 A2）

SceneResources 在 begin_upload_batch 前检查全部 live effects 的需求，隐藏、窗口外、ancestor clip 外、零 coverage 和零 opacity 均不能绕过；拒绝失效附件并保留原子重试数据。旧单记录 pack 在任何裁剪早退前拒绝 paint 或非 Round cap，不能降级成 solid/Round。baseline_scene_capabilities 的新能力始终关闭；Recording 独立 opt in，SDL 的 paint/Linear/Conic/underlay 与非 Round 产品能力均保持关闭；BUILD_TESTING 的 owner-local scope 仅用于显式候选验证。能力声明不代表完成平台验收。

### Effect Paint ABI v2（107 B1/B2 raw-affine）

独立 `effect_paint_abi_version=2` 不改变 logical scene v3、packed scene ABI v2、112-byte Effect、七个 float4 或 kind 0..5。ABI v1 的 physical float field 会丢失决定 stop 的源信息，必须拒绝，不能只换 shader 算术。下列三表全部采用 16-byte alignment；整数均为 uint32，不以 float 存索引，不借用 base 的 dash/Tooltip 参数位。保留字段必须为零。

| 表 | stride | 字段与 byte offset |
| --- | --- | --- |
| link（第二 instance stream） | 16 | foreground_field 0、underlay_field 4、arc_cap 8（Round=0 / Butt=1 / Square=2）、reserved 12 |
| field（PS storage 0） | 48 | kind 0（Linear=1 / Conic=2）、first_stop 4、stop_count 8、reserved 12；uint4 raw_domain_bits 16；uint4 affine_bits 32 |
| stop（PS storage 1） | 32 | float4 position 0（x 为位置，yzw=0）；float4 premultiplied_rgba 16 |

`0xffffffff` 是 link 的 no-field sentinel；foreground 缺省时 underlay 也必须缺省，solid 由 base.color 表达。raw domain 保存 translation 之前的 logical binary32：Linear 为 `(sx,sy,ex,ey)`，Conic 为 `(cx,cy,start,sweep)`；affine 保存 `(translation.x,translation.y,display_scale,0)` 原始 bits，不先做坐标相加或乘法。四个域分量和三个 affine 因子均以整数存取，保留 subnormal 与有符号零；不经浮点 vertex fetch。

至少两个 stop，保留重复点与域外位置。field/stop span 在 uint64 域检查后才收窄；field index 不能等于 sentinel。ABI v2 保留原 physical float admission envelope：原先要求有限的物理端点、float 差与严格正有限长度平方仍检查，不新增数值拒绝，也不将其描述为接受所有有限 Core Linear domain。Conic sweep 仍在 `(0,2pi]`。资源 limits 按实际 field48 capacity 预算；link16、stop32 与独立 cap32 保持。

Linear 采用精确 affine B 语义。`P` 为当前物理 sample 的 binary32，`S/E/T/scale` 来自原始 source bits；将这些值视为精确 dyadic，计算 `A=E-S, Q=P-scale*(T+S), N=Q·A, D=scale*(A·A)`，以 `N-stop*D` 的精确符号做右连续 stop 选择。不得回到先在 float 中变换 domain 或先做 Point<float> 局部减法。旧 Core float/double logical reference 只作普通近似兼容检查；独立 source oracle 不调用 packer、packed reference 或 shader。

共同实现与 HLSL 使用 72 个 base-2^16 digits，即 1152-bit 有符号整数。degree-four 的 `N-stop*D` 在 2^-596 格点上，保守 magnitude 小于 2^516，至多 1112 magnitude bits；符号及 25 次保守 doubling 仍小于 1138 bits。插值分数用独立整数除法/tie-even 期望验证，落在 2^-24 网格，绝对误差不超过 2^-25；这不是后续浮点 premultiplied 颜色链的逐位等价证明。颜色仍在原 Color 数值域插值，两场 source-over 后统一乘 coverage/opacity。Conic 的 raw-affine transport 不代表其角度/跨 stop 精度已认证。

link 与同一次 compact 的实际 packed Effect 一一对应，包括 solid/paint 混排；删除、隐藏、cull 和 slot reuse 同步 remap，不使用 Store ID.index 作为 GPU index。全 solid/default Round 的可见场景只走旧 pipeline，三表资源为零。可见 paint 或非 Round cap 启用新 pipeline，所有可见 Effect 都具有 link；cap-only 场景也分配、上传各一个零 field/stop 作为两个 storage binding 的最小占位，不能省略 limits 检查。占位不被 link 引用。

能力分别为 `effect_paints`、`effect_arc_caps`、`effect_linear_fields`、`effect_conic_fields`、`effect_underlays` 和独立 ABI；仅设置其中一个不能暗示其他支持。共同 preflight 扫描所有 live source，检查完整语义、物理 packed domain、stop span、整数及乘法溢出；base/link/field/stop 的实际分配 capacity（power-of-two 增长，保留同 epoch 已有容量）分别受 maximum_buffer_bytes 和 size_t 上限约束。epoch 变化不继承任何旧 GPU capacity。上限只是输入预算，不保证 create 成功。

RoundedEffectGpuResources 一次 compact 并读取同一份 geometry/material dirty，完成 base/link/field/stop packing 和上传后才 clear dirty；paint 场景的任何更新整体重打包三表，idle 零上传。SceneResources 内任意 create/upload/exception/commit 失败，包括后续 Image 上传失败，均 cancel 并使整个 attachment 失效，强制 base 与三表无新增 CPU dirty 完整重试。backend upload 成功返回前拥有 bytes；retire 在旧 device 活着时释放全部表，epoch 改变仅 abandon 旧 handles/capacity/count，foreign owner/epoch/kind 拒绝沿用。

共同 draw_binding(first,count) 同时提供实际 base/link byte offset（`first*112`、`first*16`）及 field/stop handles 和有效 count；B2 必须对两个 instance stream 使用同一个 first/count，两个 storage 从表起点绑定，不以 draw 的 first 重定位 field/stop。Recording 从实际 buffer 的 draw subrange 读取 link，并记录实际引用的 field/stop 索引与 bytes；多 draw 保持相同全局表索引。Card rounded descendant clip 将使用独立 clip ABI，不增加第四张 paint table。B1 不实现 SDL/HLSL 或 Progress 消费，不代表任何 GPU/OS 验收。

### Cap Geometry ABI v1（107 B2 数值修复，待实施）

大半径细弧存在 physical float 打包碰撞：不同 logical 输入可产生完全相同的 base/link/field/stop bytes，而正确覆盖率不同；shader 无法从已丢失的信息恢复。非 Round 接入因此另增显式 `cap_geometry_abi_version=1`，不得复用 paint ABI、重解释 reserved 字段、收紧合法输入范围或改变既有 112/48/96-byte ABI。已完成的 A/B1 验证不包含此新增合同；SDL 非 Round 在数值与原生验证通过前保持关闭。

独立 cap instance stream 每记录 32 bytes、16-byte alignment：offset 0 的四个 uint32 保存当前 logical reference 定义下 translated shape 的 x/y/width/height 原始 float bits；offset 16 的四个 uint32 保存 stroke_width/display_scale 原始 float bits及两个零。start/sweep 保留 base 原字段。使用两个整数 vertex attributes 与不插值传递，在 bit 解码前完成零/正值判断，避免 float vertex fetch 或 FTZ 先丢失正 subnormal。保留全部 Rect 分量，不把已舍入的 physical center 当作原始因子。

仅可见 nonRound 场景需要第三 instance stream；与同一次 compact 的 base/link 一一对应，其他 Effect 行写零。draw 的三个流 offset 分别为 first×112/16/32，first_instance=0；PS storage 仍为 field/stop 两个，不增加第四张 paint 表。纯 solid/default-Round 零额外资源，只有 Round paint 的场景保持两个流。缺失或未知 cap ABI 的 backend 在 begin 前拒绝全部 live nonRound 需求。

cap buffer 纳入同一 capacity/limits、owner/epoch、失败失效、无新增 CPU dirty 重试、retire 与 idle 合同；任何表或后续资源失败均一起重试。保守 CPU bounds/cull/draw envelope 必须覆盖合法 AA 片元，不改变实际 shape 或 ancestor hard clip。数值求解分离 R/H，以 query-local 补偿及有界指数/精度路径处理运算，保持暴露联合外边界、单次 coverage 和固定一物理像素 AA；不得以任意 epsilon 填平正宽 gap、无限细分或让 NaN 静默清空。

实施必须分别验证独立 scalar 期望、真实 packed bits/draw domain 和真实 GPU RGBA；完整碰撞反例要求新 stream 保留差异并输出各自正确结果。检查 shader roundtrip 中的算术保持性、实际编译/运行成本、nonzero first、混排、失败/重试与旧 Round/dash/Tooltip。Windows/D3D12 与 Linux/Vulkan 分别记录，不能用生成二进制或 CPU reference 代替实际 GPU；cap 修复不构成 paint field 全域无损精度声明。

共同源码已加入独立 cap packing/buffer/Recording 及回归源码；非 Round bounds 使用向外包络的 double endpoints，在 viewport/ancestor clip 截取后才转换 draw 坐标，Square 使用保守 R+2H envelope。公开 Rect 的向外转换也检查实际 float origin+extent，store 的 compact/update 使用宽 endpoints。compose 与 upload 必须传同一个 nonRound logical AA guard（至少 max(1,1/display_scale)），尤其 scale 小于 1 的 fractional ancestor clip；不得在 OrderedScene 生成后改变 compact 映射。它只改变 draw/cull envelope，不扩大 shape 或 hard clip。该源码阶段尚未编译运行，既有 A/B1 数量不包含新增合同；53.1/53.2 继续未完成。

### SDL/HLSL B2 候选与验证边界

SDL 的 link 为 VERTEX buffer，field/stop 为 GRAPHICS_STORAGE_READ buffer；typed tombstone、owner/epoch、范围与源 bytes ownership 归同一 upload batch。Round paint 候选使用 base112/link16 两条实例流、七 float4 加 uint4 属性与两个 fragment storage；field/stop 从表起点绑定，draw first_instance=0。冷启动旧 solid/default Round 不创建新 pipeline/表，恢复旧场景释放三表，已创建 pipeline 缓存至析构。新 pipeline 创建失败使事务失败。

新旧 entry 共用 `rounded_effect_coverage.hlsli` 的旧 Round/dash/Tooltip/阴影/outline 和一物理像素 AA；hard clip 保持左上包含、右下排除。paint 两场在 premultiplied 域插值和 source-over 后，仅乘一次 coverage/opacity，再输出 straight RGBA。Linear 按上述 raw-affine/1152-bit 合同精确选择 stop，任意有限、相邻和重复 stop 不设新范围限制。Conic 候选使用 224-bit 坐标同时选择 stop 和求分数，保留 true-tau wrap/full sentinel/center0；其全域算术误差、跨 stop 判定与 unresolved 的有界处置仍未认证，因此不能据此打开产品能力。cap union 源码保留为独立候选，本阶段不生成、反射或嵌入 cap entry；SDL 明确拒绝 cap pipeline，disabled cap suite 不属于本阶段可执行 GPU 验收。

Linear 必须独立记录冷 pipeline/JIT、共同 pack/upload、submit/fence/readback、source oracle 与总 wall time；submit/fence/readback 的 CPU 时间不是独立 GPU kernel 时间或 FPS。先测有界小矩阵，再显式测整 1080p 的单 Linear 与两个半透明 Linear。保留 55 秒 workload/60 秒外部 guard，不以提高超时、只测同色或不透明双场代替成本验证。候选阶段全部产品 flags 保持关闭，数值与平台证据按实际运行分别记录。

`tools/effect_shader_closure.py --write-lock` 显式生成 schema2 JSON/YAML-subset lock，包含每文件 LF SHA256、长度、排序 closure SHA256 及各 stage 的编译定义。vertex/fragment 分别显式定义 `RYNUI_VERTEX_SHADER=1` / `RYNUI_FRAGMENT_SHADER=1`；paint storage 与采样入口只编入 fragment，避免 vertex 模块留下未使用的 descriptor 声明。验证递归 quoted includes，拒绝循环、越界、漏锁、stage recipe 与 CMake DEPENDS 漂移；新旧各 stage 编译依赖完整 closure、lock、validator。交接不生成 lock/binary，父在最终格式化后用锁定 BUNDLED shadercross/DXC 生成。JSON 核对 IO/资源数量，`tools/verify_effect_paint_spirv.py` 从真实 SPIR-V 检查整数/Flat、set2 storage0/1、stride/offset；不将其描述为 DXIL reflection。

`read_frame_rgba` 返回 offscreen render/download/fence 后的 tight owned RGBA framebuffer，BGRA 显式 swizzle、保留 alpha、不解预乘，失败清空输出、RAII 清理；正常 frame tick 不等待 fence。native fixture hidden/no-focus 并核验真实 driver/format。BUILD_TESTING 的 field-upload 与 finish 一次性故障 seam 只验证真实 adapter 的取消和完整重传，不是 SDK OOM/device-loss 证明。Windows/D3D12 与 Linux/private Wayland/Vulkan 均须实际执行；生成另一格式不算对应平台通过。原始 RGBA/log 只写 ignored out。

测试构建提供 owner-local pending paint/cap validation scope，退出后恢复产品默认拒绝；paint scope 不声明 cap。cap entry 本阶段未生成，scope 不绕过 SDL 的明确 pipeline 拒绝。Conic 任意窄/重复 stop 的误差分段判定、224-bit cap 谓词及暴露边界认证、实际编译与 GPU 仍分别追踪；普通矩阵通过不能开启产品能力。宿主通过内部 WindowComponentServices 的 display_scale scalar setter/accessor 在 compose 前发布实际上传 scale，DPI 变化重发 projection，Core 不依赖 renderer/SDL/SceneDeviceMetrics。

## 后续组件与 backend 的接入清单

- 组件：typed Props/slots、Prop<T> 与 Theme/Component Token；只发布 logical scene 和最小 dirty 范围，通过共同 SceneResources/SceneBackend 上传呈现；不得读取 GPU capability 或设置 transfer layout。
- backend：显式声明必需能力/版本与输入限制，实现源数据复制、opaque handle 的 kind/owner/epoch、begin/commit/cancel、ordered draws 与 submitted/deferred/failed；保持成功 upload 后 caller memory 可立即释放。
- 恢复：从同一 CPU scene/atlas 重建，保留组件/editor 身份；旧代际附件不可提交，不能通过新 device 释放旧 handle。
- 验证：以 Recording 检查真实 bytes、范围、顺序、失败与 epoch；每个真实 GPU/OS 另留 shader、字体、输入/DPI、窗口与生命周期证据。第二真实 backend、新平台宿主及自动 device-loss 等在各自 change 中实现。

## Image 扩展（107）

Image 保持 logical scene v3 和既有 packed ABI v2 三类记录不变；新增 `SceneDrawKind::image` 与独立 `image_packed_abi_version=2`。Core 的 ImageStore 以 index/generation 保留不可变 ImagePixels、logical bounds/clip/pivot/顺时针旋转、flip 与 opacity，不含 renderer handle。Image draw 的 first_instance 为 slot index、instance_count 固定 1、atlas_page 字段在此 kind 表示 slot generation；不得跨图片合并。ImageGpuInstance 独立为 112 bytes、16-byte alignment，七个 float4 offset 为 0/16/32/48/64/80/96，分别是 NDC rect、UV、clip、tint、translation/opacity、rotation basis、可选圆形裁剪的 NDC rectangle；圆形覆盖率在独立 shader 中计算，祖先矩形 clip 保持独立。几何投影与 Glyph 共用数学合同，采样是独立 RGBA8 shader，绝不经过 R8 atlas。

图像像素为左上原点 RGBA8 straight alpha；RGB 保留输入的编码值，无 ICC/gamma 转换，使用已有 source-alpha blending。PNG 的 16-bit 输入归一至 8-bit；JPEG EXIF 方向不自动应用，应用可先规范化元数据/像素。透明像素的 RGB 保留，解码器不预乘。ImagePixels 校验正尺寸、行宽/stride、实际最后一行、溢出与配置上限，拥有源 bytes；decoder 不读取路径或 URL。默认上限为边长 16384、解码 256 MiB、编码 64 MiB。

SceneCpuData 在旧五个 aggregate 成员后增加可选 ImageStore。SceneResources 共同 preflight 在 begin 前校验 Image ABI=2、images/rgba8_sampling、buffer bytes 与 texture extent；无 Image 数据的旧调用保持有效。ImageGpuResources 在共同事务内上传独立 texture/buffer，纹理源只携带 bytes/offset/row stride/width/height。SDL backend 逐行复制、清零 padding（row 256 bytes、batch offset 512），pixels_per_row 按 RGBA pixels 计算；Recording 保留紧凑 RGBA bytes。上传返回成功后拥有所有源数据。附件检查 generation，事务失败失效附件并强制重传；epoch 变化 abandon 旧 device handle，再从同一 CPU Store 重建；显式 retire 仍在 backend/device 存活时释放。无变化不重传，geometry-only 更新只上传 instance。平台 shader/真实颜色验证独立于 Recording 结果。

### Retained subtree paint opacity

Core 内部 Node 的 paint opacity 是祖先相乘的纯绘制包络，与输入可达性独立。RetainedSurfaceService 保留未乘包络的 Quad 与 RoundedEffect 来源；创建、包络变化和 range remap 后从来源重新生成材质，零透明度恢复不会丢失原始值。Text/Icon/Input 的 glyph、选区、光标和直接 effect 使用同一祖先因子。Image 主图、PreviewGroup 与 Upload 缩略图及预览 canvas 发布 ImageInstance 时应用一次 content opacity 与 paint opacity 的乘积；其背景/焦点/遮罩由共同 surface 应用包络，不把已乘值再交给 renderer 全局缩放。该机制不创建下一帧请求；动画宿主仍须分别执行本帧 invalidation 与下一帧 deadline。输入禁用、焦点、capture、preedit 和 popup 的清理由组件的 input boundary 完成。logical CPU scene v3、既有 Quad/Glyph/RoundedEffect packed GPU ABI v2 和 Image 独立 ABI v2/112 bytes 均不变。

圆角 Image 的 logical 分片具有互不重叠的实际 bounds 和相对于完整原图的 `source_uv`，所有片段保留同一个 pivot；packer 将 UV 子范围写入已有 `uv_rect` 并在原图坐标中翻转。圆角 coverage 仍使用已有 circle clip；矩形邻接处依靠共享边的三角形光栅化规则，不以 epsilon 裁剪制造缝隙。共同 ImageGpuResources 的 texture lease 仅在自身 device/owner/epoch 内按同一不可变 ImagePixels 身份共享；source 变更、upload retry、retire 与 abandon_device 保持独立代际和事务资格。该扩展不改变 logical scene v3 或任何 packed ABI。

## Scene Composition v1（107）

内部 program 仅包含 DrawRange、BeginMaskedGroup、EndMaskedGroup、Backdrop，绑定最终 OrderedScene 的 revision 和完整 commands；primitive 的 first/count、logical3/packed2 与 Quad48/Glyph96/Effect112/Image112 不变。SceneCpuData 尾部可传 `composition_scene`，上传前冻结最终可见 program；遗漏或混配会拒绝 attachment。旧解释器拒绝带 composition 的 stream。Core Node 只持有 logical radius/几何，hit snapshot 保存不可变 rounded clip 链；窗口层停止该 clip 链，保持既有 owner 和输入限制。

独立 clip ABI1/filter ABI1 的 16-byte aligned sidecar：PassViewport32（full viewport@0、crop origin/extent@16）、DrawCoverageControl16（uint role@0）、RoundedClip64（shape@0、radius/AA@16、hard clip@32、source origin/extent@48）、GaussianFilter32（source@0、uint axis/radius/first/count@16）、GaussianWeight16（float weight@0）、BackdropComposite96（clip64、PM tint@64、activation@80）。self surface 的 owner shape 通过独立 RoundedClip uniform 提供，不占用 primitive 或 Effect Paint/cap bytes。未知版本/role、失配的 draw stream 与不平衡组在 begin 前拒绝。

全部旧 instances 按完整窗口 metrics 打包，新 pass 才映射 crop；直边和圆角 draw support 外延 1 physical px，真实 ancestor hard clip 不扩大。composition 的新物理 shape 坐标必须保留 half-pixel 表示能力，不向无 composition 的旧 logical scene 增加该约束。

SDL 使用 RGBA8_UNORM、PM 中间 target、sample_count1；primitive 保持 straight source-over，masked PM 输出使用 ONE 合成。Backdrop 冻结 root 的当前 prefix 到独立 B，完整窗口作为保守 halo ROI，H/F 两遍 normalized Gaussian 后以禁用 blending 的 replacement 写回；不读写同一 texture，不跨帧缓存像素。绘制和 RGBA readback 调用同一个 program 执行器。

SDL 的实际接受上限为 target 16384×16384、全部未完成 frame 加本次容量 512 MiB、2 frame sets、32 层、4096 live requests、1048576 ops、Gaussian radius256、weights16 MiB；这些不是显存可用保证。共同 preflight 检查所有 live 请求，包括 hidden/opacity0；target 按实际容量计预算。资源与旧 primitive/Image 共享上传事务，失败失效附件并完整重试；frame fence 完成前不能重写同一 target，owner/epoch 改变失效资格，关闭后的安全资源释放回冷路径。冷场景不创建 composition target、weights 或 pipeline。shader closure/反射和 Recording 属于必要源码/资源检查，Windows D3D12 与 Linux Vulkan 的真实 GPU 结果分别记录。
