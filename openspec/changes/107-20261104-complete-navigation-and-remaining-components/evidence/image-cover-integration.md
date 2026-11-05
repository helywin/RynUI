# Image 共享阶段主线集成：Linux

- 交付前 main：A `42d55bcce6f1b41ed94c76f2d80ba1b58b2d9dc6` → B `20e67d510535d5accd23c1ef70989fa1ef7a3aa0`，两阶段各自验证后已立即 fetch/rebase/push，并核对实际远端一致、0/0。
- 唯一来源为 `7dab3ea53b9fb64fbb9b2b5dbbda09c495237d4a`（父 `4d722fbc19fbe0a604530d2de578556dbf368b85`），合入 14 文件增量；[来源结果](image-cover-linux.md) 仅作来源记录，下面为当前 main 重新验证。
- 保留 Node paint opacity/count/lifecycle、A 位移所有权和所有现有组件；分片使用 disjoint bounds/source_uv、统一 pivot 和 renderer/common texture lease。logical scene v3、Quad/Glyph/RoundedEffect packed ABI v2、Image ABI v2/112 bytes、shader/backend API 不变。
- 不导入 `0cd4817b`、Card/Empty/Statistic/Select/Skeleton WIP 或尚未交接的 Image/Form delta；只提供内部 cover 策略，不据此完成 Card 任务。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config；构建命令均 `-j2`。HEADLESS targets：`rynui_image_component_tests rynui_image_resource_tests rynui_image_rounding_tests rynui_image_lazy_preview_tests rynui_image_public_api_compile`。
- Debug：`ctest --preset linux-gcc-headless-debug --output-on-failure -R '^rynui\.image_(component_.*|resource_.*|rounding|lazy_preview)$'` **28/28**；其中 rounding/resource **810 条检查**、lazy preview **79 条检查**，无断言失败或 Not Run。
- Release 同一 targets 与选择器 **28/28**，rounding/resource **810 条检查**、lazy preview **79 条检查**；Image 公开 API object 两配置构建成功。两配置均 0 失败/Not Run。
- 实际 HEADLESS Core include/link 正负 guards、目录生成 check/self-test **3/3**；clang-format 22.1.3 **1026 文件、0 失败**。
- `linux-gcc` Debug 构建 `rynui_image_rounding_gpu_tests`；`ctest --preset linux-gcc-debug -V -R '^rynui\.image_rounding_vulkan_private$'` **1/1**，实际 Vulkan 私有 Mutter/Wayland，**2,449,208 条 RGBA/readback 检查**。覆盖四种缩放、分数几何、四角/上角、clamp、UV/flip/统一 pivot 旋转、祖先 clip、四种 fit、自有背景/边框及 opacity 1/.5/0/1；内部接缝逐像素精确对完整图参考，无容差放宽。
- `openspec doctor --json` healthy、`openspec validate --all --strict --no-interactive` **107/107**、`git diff --check` 通过。C 交付提交由本文件首次加入的 Git 历史标识，完整 SHA 见交付回执。
- change107 保持 **68/135**，不新增完成 checkbox；原 97 samples、458 frozen IDs、5 份完整 66 字段 Theme golden 保留。原始日志和读回仅 ignored out。
- 未验证 Windows/MSVC/D3D12、物理 IME/AT、跨实际显示器或用户桌面；平台汇总项不关闭。
