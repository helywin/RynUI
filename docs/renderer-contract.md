# Renderer 合同

035 的 P0 建立共同 renderer 边界；实现与验收状态以 change 的 tasks 和 evidence 为准。Android、iOS、Web 与新 GPU renderer 属于后续工作。

## Packed scene ABI v1

现有 scene 是 CPU 保留的、可重建的 packed 数据。布局使用左上原点、x 向右/y 向下的 logical 坐标；QuadScene/GlyphScene 在 Core 转换为下表的数据。不能将目录移动描述为全 logical scene 迁移。

| 数据 | 合同 |
| --- | --- |
| Quad | 48 bytes、16-byte alignment；rect 为 NDC `(left, top, width, negative-height)`，translation 为 NDC 向量；圆角为最短边的 0..0.5 比值 |
| Glyph | `GlyphInstance` 固定 float packing；rect/clip 为 NDC，UV 左上原点、范围 0..1，translation 与 opacity 存在 instance 数据中 |
| RoundedEffect | CPU store 保留 logical；共同 resources 按 device metrics 打包 NDC rect/clip 与物理 pixel 尺寸、半径、效果参数；格式由 `RoundedEffectGpuInstance` static_assert 固定 |
| 颜色 | RGBA 浮点 token 值直接传入现有 shader；RGB 使用 source-alpha 混合，alpha 使用 `one + one-minus-source-alpha`；现有路径未增加统一线性/sRGB 转换 |
| 顺序 | OrderedScene 的 Quad/Glyph/RoundedEffect 顺序必须保持；仅已有场景合同允许合并相邻兼容 draw |

NDC x 向右、y 向上；完整视口范围 [-1, 1]。clip 边界、atlas UV 和 glyph padding 保持现有 shader 合同。backend 在消费边界转换 GPU API 特有的坐标、格式、传输对齐，不能让组件选择 shader 或 OS 类型。完整 logical scene 迁移需另立版本和验收。

## 构建边界

`RYNUI_PLATFORM_BACKEND` 支持 SDL 与 HEADLESS；`RYNUI_RENDER_BACKENDS` 是非空的已知 renderer 列表 SDL_GPU、RECORDING。SDL 可与 SDL_GPU、RECORDING 或两者构建；HEADLESS 只支持 RECORDING。选择是编译期，P0 不提供运行时多后端选择器。当前原生 examples 需要 SDL_GPU，其他配置应关闭 examples。

`windows-msvc-headless` 构建生产 Core 与共同资源测试，完全跳过 SDL、libdecor、shadercross/DXC、shader 生成和默认系统字体服务。FreeType、HarfBuzz、utf8proc 和锁定验证字体仍是实际依赖。该 preset 验证后端隔离，不代表新操作系统支持。默认 Windows/MSVC 和既有 Linux presets 保持原生路径。

## 资源与上传

`SceneResources` 绑定唯一 SceneBackend owner 和设备 epoch，拥有 Quad/Glyph/Effect resources。调用 `synchronize(SceneCpuData)` 执行 begin → 共同资源同步 → commit；任何失败/异常都 cancel、恢复 CPU dirty 状态并失效附件，成功 commit 后才能 `attach`/呈现。低层同步清理的 dirty ranges 只在共同事务成功后有效；示例不得绕过共同事务。CPU OrderedScene 借用至提交完成，期间不得改变/销毁；附件 weak stamp 防止资源销毁、失败或 epoch 改变后访问旧资源。

backend 在上传方法返回成功前复制/拥有源 bytes；调用者的 temporary staging 可立即释放。commit 仅表示接受命令，不表示 GPU 已完成。纹理行对齐由 GlyphGpuApi capability 提供：SDL GPU 为 256，Recording 为 1，传输偏移对齐由具体 backend 校验。UI Core 与 atlas 不包含 SDL transfer packing。

显式 retire 在旧 device 存活时释放资源并使所有附件失效；backend epoch 已改变时，仅 abandon 旧代际 handle，再从 CPU scene 全量重建，不能经新 device release。backend 必须比 SceneResources 活得更久。SDL 重建采用销毁资源/renderer 后构造新 renderer，未添加自动 device-loss 恢复。

Recording 实际复制 buffer/texture 数据、校验范围和类型、保持 handle tombstone、记录 draw 消费的 instance bytes。失败注入覆盖 begin/create/upload/commit/exception；reset 显式增加设备 epoch。该实现不栅格化，不作为 GPU 性能或视觉等价证据。
