# Calendar / Carousel 主线集成验收

2026-11-05，main 基线 `135ba70b74673bc6d7b34d1d83aa414fd0db3607`。顺序语义合入 `92c61feb4119848775128d2895a7b4208c2152e8`、`bd5353cb91d04d105a1a9011b9b7440bfd61283b`；源工作树只读，源 [Calendar](calendar-linux.md) / [Carousel](carousel-linux.md) 证据不计为本次主线结果。

共同修复：Quad/RoundedEffect 保留未缩放来源，创建及局部更新跟踪 paint 包络；补齐同步前由非 1 恢复为 1 的待同步状态、范围迁移、嵌套、零值恢复、重挂父节点与析构计数。Text/Icon/Input/TextArea 和 Image 主图/预览/PreviewGroup/Upload 缩略图各应用一次祖先因子。输入门控独立于 outgoing/incoming 绘制；未增加 renderer 全局 alpha 或组件私有上传。logical scene v3、Quad/Glyph/RoundedEffect ABI v2、Image ABI v2/112 bytes、共同资源事务与旧输入/Modal/Form 清理保持。

实际平台 Linux/GNU，Ninja Multi-Config，所有构建 `-j2`。`linux-gcc-headless` Debug/Release 分别构建 15 个相关 target；定向 CTest 各 **38/38，0 失败**，包括源 35 项以及 Image/媒体 opacity、Form 私有控件/嵌套 Item、Tree/Form 组合。新增全 glyph（含 TextArea）断言后，仅该受影响项各复测 **1/1**，不重复计入 38 个不同检查。公开 Calendar/Carousel examples 由实际合同 target 编译并挂载。真实 HEADLESS configure/build 和 backend boundary 守卫通过。

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_calendar_tests rynui_carousel_tests rynui_paint_opacity_tests rynui_paint_opacity_integration_tests rynui_calendar_gallery_tests rynui_carousel_gallery_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests rynui_portable_theme_public_api rynui_portable_logical_scene_packing rynui_portable_button_scene_service rynui_portable_pointer_route rynui_portable_rounded_effect_scene rynui_form_integration_tests rynui_tree_form_integration_tests -j2
ctest --preset linux-gcc-headless-{debug,release} --output-on-failure -R '^rynui.(carousel_|calendar_|paint_opacity($|_integration$)|theme_algorithm$|theme_runtime$|form_integration$|tree_form_integration$|portable.(theme_public_api|logical_scene_packing|button_scene_service|pointer_route|rounded_effect_scene|backend_boundaries)$|gallery_sample_source_generated$)'
```

Linux `linux-gcc` Release：Gallery、opacity/Image GPU targets 构建通过。通过既有 `run_background_wayland.py` 的独立 headless Wayland/private DBus，实际 Vulkan/SPIR-V 读回 **2179 checks**：真实 Button/Input/TextArea/effect/Text glyph 与 Image 的半透明、零/恢复、旋转预览嵌套 .25、双面板 Fade 中点与嵌套合成。2179 包含 glyph 区域采样读取和颜色断言；RGB 容限保持 10/255，坐标选真实像素/纹理中心。原 Image 独立资源回归 **99 checks** 通过。Linux ordered platform input/text events/SDL adapter 另 **3/3** 通过；不是物理输入或 OS drop 证据。

```text
python tools/run_background_wayland.py --log-dir out/calendar-carousel-integration/gpu-opacity-final --timeout 60 -- out/build/linux-gcc/tests/Release/rynui_paint_opacity_gpu_tests out/calendar-carousel-integration/gpu-opacity-final/readback
python tools/run_background_wayland.py --log-dir out/calendar-carousel-integration/gpu-image-regression --timeout 60 -- out/build/linux-gcc/tests/Release/rynui_image_gpu_tests out/calendar-carousel-integration/gpu-image-regression/readback
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --log-dir out/calendar-carousel-integration/native-final --preset linux-gcc --configuration Release --timeout 120 --page calendar --page carousel --page image --page button
```

最终原生序列 **4/4 cases、596 checks、154 submitted frames、154 次实际 Wayland frame callback，0 失败**，使用系统字体及实际 host input area/cursor 同步。覆盖日期键盘/Enter、年/月/header/Today、locale/range/mini、重叠 cell 草稿/preedit、Carousel arrows/dots/ScrollX/Fade 中点、drag/cancel、autoplay/pause/恢复与离页，以及 Image 像素/预览/源变化。有限验收 host 主动请求采样帧；deadline/暂停/idle 零帧由平台通用合同单独验证。106 callback、pacing 和正常 SDL pump 未改动。首轮 Calendar 脚本漏 Enter，按实际键盘合同修正；媒体 fixture 补齐几何发布后的延后源同步。最终无未处理失败，未削弱颜色/中点断言。

目录/source writer/check/self-test：**97 samples**，原 93 全保留，458 个 frozen legacy IDs 的 inventory 逐字节不变。5 份完整 Theme golden 均 **66** 个顶层字段，原 64 字段（identity 除外）全部保护；旧 TokenIdentity 名称/ordinal 和公开 ThemeConfig 聚合顺序不变，新增两个域参与 equality/hash/继承/失效。clang-format 22.1.3 检查 1013 个自有源，0 失败；doctor healthy、strict **107/107**、diff check 通过。

本阶段仅关闭 28.1/28.2、30.1/30.2，进度 **60/135**；63.2 保持完成，63.3、64.*、65.1 保持开放。Windows/MSVC/D3D12、物理系统 IME/AT、实际桌面和跨物理显示器未验证。无用户桌面窗口、物理鼠标键盘、fallback 或系统 proxy 改动；原始 log/JSON/readback/二进制仅在忽略的 out。
