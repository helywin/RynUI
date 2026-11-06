# Image 圆角与预览共同阶段（Linux）

以下为来源提交 `7dab3ea53b9fb64fbb9b2b5dbbda09c495237d4a` 的独立工作树结果；当前 main 重新验证见 [Image 共享阶段集成](image-cover-integration.md)。

- 基线 main `4d722fbc`；语义吸收 `0cd4817b` 的 ImageShape::RoundedRectangle、preview(false) 惰性 Modal 及文档需求，未覆盖新 main 的 paint opacity。原七个全图 clip 实现已替换，不应再单独合入该旧共享提交。
- GCC `linux-gcc-headless`、Ninja Multi-Config、`-j2`：Debug/Release 的 Image component/resource/rounding/lazy-preview **各 28/28 CTest**；公开 Image API object 在两配置构建。新分片/资源合同 **各 810 断言**，惰性预览 **各 79 断言**。
- `linux-gcc` Debug，私有 Mutter/Wayland/独立 D-Bus 与真实 Vulkan：`rynui_image_rounding_gpu_tests` **2,449,208 检查通过**。覆盖 1/1.25/1.5/2 缩放、分数几何、上角/四角及 radius clamp、祖先矩形 clip、RGBA 半透明分片接缝、UV/flip/统一 pivot 旋转、透明度中点/零/恢复。内部接缝与完整图参考逐像素精确比较；无 epsilon clip 或放宽颜色容差。
- 几何端点按同一边界拆分；宽封面暴露的 source UV 浮点越界已用源区间端点计算/clamp 修复，并增加零、极窄和宽分数尺寸合同。GPU 角点断言选取确定在内部/外部的像素，避免把边界抗锯齿覆盖误认成实心区域。
- renderer/common texture lease 按同不可变 pixels 在本资源 owner/device/epoch 内共享；验证单纹理/单 source upload、片段独立退休、source 变化、失败 retry、DPI idle、重复退休、abandon_device，以及实例 buffer 先于最终 texture 的析构顺序。
- `preview(false)` 不挂载 Modal/actions/boundary，false→true 惰性构建、关闭/禁用清 preedit/焦点、反复启用保留 toolbar 草稿、过期 Modal 拒绝开窗、初始挂载事务启用通过；actions 工厂抛异常后局部回滚、无残余边界。
- 必要命令：`cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_image_{component,resource,rounding,lazy_preview}_tests rynui_image_public_api_compile -j2`；`ctest --test-dir out/build/linux-gcc-headless -C <Debug|Release> --output-on-failure -R '^rynui\.image_(component_.*|resource_.*|rounding|lazy_preview)$'`；`tools/run_background_wayland.py --timeout 60 -- <GPU test> <out directory>`。
- logical scene v3、原 Quad/Glyph/Effect ABI v2、Image ABI v2/112 bytes、shader/backend API 均不变；Core 不依赖 renderer/SDL。无任意后代 mask；Card 只向直接默认 Rectangle Image 传内部上角策略，显式 shape 优先，预览独立。上角策略同时作用于 Image 自有背景/hover/border；四种 intrinsic fit 与 Theme 背景的半透明真实组件读回通过。
- 原始日志和读回仅在忽略的 out。Windows/MSVC/D3D12、物理 IME/AT/跨实际显示器未验证；不关闭分类平台汇总项。

- 来源 `f757e13c97b2342fc82d3670456e8556b7590664` 的后续独立 test-only 扩展在透明 FBO 上逐像素核对 RGB 与 alpha（128/64/0/恢复），防止不透明背景掩盖重复 blend。相同四个缩放与透明度序列的真实 Vulkan 合计 **2,581,848 检查**，`ctest --test-dir out/build/linux-gcc -C Debug -R '^rynui.image_rounding_vulkan_private$'` **1/1**；不修改 shader、ABI 或资源实现。
- 来源 `071f6132718a7cfcd3556440cc7e4bd6d5629fa4` 的实现分支在 `7dab3ea5 → f757e13c → 8e6151b8` 共同实现上补跑现有默认 preview=true 的独立 Image→Button 原生序列：`tools/run_gallery_page_acceptance.py <linux-gcc Debug Gallery> --preset linux-gcc --configuration Debug --timeout 120 --log-dir <out> --page ant.component.image --page ant.component.button`。私有 Wayland/Vulkan **2 页、120 检查、23 提交帧通过**，实际图片/预览工具栏、变换/键盘和页面退休合同保持；不是 Card preview=false 或 HEADLESS 的替代计数。
