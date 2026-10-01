# Design

## Context

动机见 proposal.md。035 的 SceneResources 已拥有共同上传事务、owner/epoch 和失败恢复；Effect 已有 logical store 与打包缓存。QuadGpuBuffer 仍位于 graphics，GlyphInstance 的 shader stride 直接约束 CPU scene，组件重复实现 NDC 数学。

## Goals / Non-Goals

Goals：CPU Quad 的 bounds 为 logical x/y/width/height、translation 为 logical 位移、corner_radius 为 logical 长度；CPU Glyph 的 position_size 为 logical x/y/width/height、clip_bounds 为 logical left/top/right/bottom、translation_opacity 的前两项为 logical 位移。保留 UV、颜色、opacity 与绘制索引语义。CPU 数组便于现有 retained store 的小范围比较，但其内存布局不再承诺 shader 兼容。

Non-Goals：不移动 Effect 的全部参考栅格数学，不改变文本 shaping/rasterization，不引入 backend 自动选择或新平台。

## Decisions

### 1. 独立 CPU scene 与 GPU ABI 类型

CPU 合同为 logical scene v2；`renderer/common/scene_packing` 定义 QuadGpuInstance（48 bytes）、GlyphGpuInstance（80 bytes）与 attribute metadata，维持 packed GPU ABI v1 的 offset、NDC y 翻转、negative height 和圆角归一化。移除 CPU 类型的 shader layout assertion。独立类型防止直接把 CPU bytes 上传；保留原类型再隐式转换容易静默混用，故不采用。

### 2. 显式 metrics 与共同资源缓存

沿用现有 `RoundedEffectDeviceMetrics` 的 pixel extent/display_scale 作为 Scene device metrics，logical viewport = pixel extent / display_scale。QuadGpuBuffer 移入 renderer/common，GlyphGpuResources 增加显式 metrics 参数。每个资源保存最近成功上传的 metrics；首次、增长、metrics 变化全量 pack/upload，普通更新只 pack 合并后的 dirty ranges。不得因 resize 改写 CPU scene。staging vector 重用容量；不在每次 idle/material update 分配完整临时场景。

整个 SceneResources 在开始上传前校验 metrics；失败仍使 attachment 无效，并恢复全量 dirty，metrics 的成功缓存不能跳过重试。独立资源 helper 的失败必须保留 dirty/metrics 失效信息，避免不经过 SceneResources 时丢失 resize 重试。

### 3. 字形 physical raster alignment 留在 Core

字体 density、quarter-pixel phase、bearing 和 atlas padding 继续决定 logical glyph bounds。这是字体栅格合同而非 shader 坐标合同。共同 packer 仅把已有 logical bounds/clip/translation 转成 NDC，不再 round 字形或重栅格化。DPI 变化导致字体重建仍由现有文本服务负责；只有 viewport/metrics 变化的打包不得触发 shaping 或 atlas 更新。

### 4. Core 只发布 dirty CPU ranges

移除 RetainedSurfaceService 的 GPU 同步入口；QuadScene::sync_dirty 只写 store 和 dirty ranges。legacy minimal renderer 从调用方同步共同 GPU helper。所有 Quad/Glyph producer 统一 logical 合同，SDL/Recording 的 stride 和 offset 使用 packed 类型。保持公开 Props/slots/Theme 接口不变。

## Risks / Trade-offs

- [坐标或圆角重复转换] → literal 数学断言及真实组件 CPU logical 断言，SDL shader 文件不改。
- [metrics 更新上传失败留下旧投影] → Recording 注入 upload/commit failure，检查重试后的完整 bytes、CPU 不变和 epoch 恢复。
- [CPU/GPU 类型大小碰巧相同掩盖直接上传] → GPU helper 只接受 logical store 后显式 pack；renderer 只依赖 packed metadata，Core 禁止 renderer includes 的现有构建 guard 保留。
- [新增 staging 影响热路径分配] → 重用 vector，保留现有 interaction/allocation benchmark 验收。
- [Linux 原生行为缺乏本机证据] → 单列 Linux checkbox；Windows 不替代该项。

## Migration Plan

先完成规划校验并提交；随后一次连贯类型迁移与 producer/consumer 迁移，运行有意义的 packing/scene/资源测试后提交；再运行 Windows 上平台通用 Debug/Release 验收及文档校验并提交；Windows SDL Debug/Release 真实窗口分别记录，Linux 保留待验收。

正式构建继续通过 Ninja Multi-Config 的 `windows-msvc-headless` 与 `windows-msvc` preset，Windows 使用 MSVC。平台通用完整逻辑 CTest 在 Windows 完成一次即可；native suite 中 GPU、平台资源与窗口行为仅代表 Windows，Linux 使用自身原生 preset 独立完成。回退以本阶段 commit 为单位，不能混用 v1 CPU producer 与 v2 packer。
