# Design

## Context

024 的五进程实测显示 `frame_layout_us` 约 14 ms，但它包括 `TextComponentHost::layout_and_synchronize`、参与者 geometry/fragment、effect compact、文本 fragment 和 scene composer；命中刷新仅约 5 µs。Gallery 每次滚动仍遍历文档子树写入 translation，累积访问 222,443 个 Node。现有外层计时不足以判断哪段首先值得优化。

## Goals / Non-Goals

**Goals:** 将同步外层拆成互斥子阶段，并记录 mounted text 访问、实际同步、离屏跳过、geometry patch/rebuild 与 participant 工作量；使用相同真实窗口场景比较五进程中位数。选择贡献最大且能保持现有语义的重复工作，先建立失败与等价回归，再改实现。

**Non-Goals:** 不把 CPU 阶段时长当作 GPU 时间；不重写滚动坐标体系、公开组件接口或 GPU renderer。

## Decisions

1. **先观测再改算法。** 在原有 `--scroll-acceptance` 输出中增加命名稳定的累积时间与计数；每个阶段以同一帧数为分母，明确计时口径。先运行五个独立 Release D3D12 进程，并保存原始数值。
2. **将 Input 文本的容器平移转为已有 post-raster patch。** 五进程基线明确 `InputComponentHost` 中三个文本图层随每个 Input 的全局 viewport origin 变化而全部走 `replace_text`：滚动 240 帧、9 个 Input、6,480 次 geometry rebuild、零 patch，约 13.94 ms/帧。保持 Input 的 world viewport、clip、caret、effect 与 surface 几何现有算法；只让 `GlyphPlacement.origin_pixels` 使用未平移的 viewport Node bounds，再把该 Node 的 translation 与已按 display scale 对齐的 Input 水平滚动量一次交给 `TextSceneService::set_phase_preserving_scroll_translation`。仅容器平移参与 raster phase 分离，剩余残差放入 `GlyphPlacement.translation_pixels`；水平滚动 offset 原样加到 snapped 平移，避免一帧两次更改 scroll translation。各图层分别调用，以保留 base/selected/placeholder 的独立身份、material 和 clip。
3. **以现有完整重建为位置 oracle。** 测试用 1.0、1.25、1.5、2.0 scale，比较整数物理像素与分数位移后可见 glyph 的位置、clip、phase、三图层顺序和 hit bounds；内容/字体/宽度/clip 变化仍必须走原有失效路径。整数物理像素滚动在 warmed scene 上要求 geometry rebuild 不增加、patch 增加。离屏后重新进入视口还需同步最新内容。
4. **前后比较。** 保持机器、preset、窗口、自动滚动步骤及 telemetry 一致。优化后可能在 1.8 秒验收结束前出现更多非滚动动画帧，因此计时与 renderer 计数限定自动滚动开始后的前 240 个提交帧；旧基线也恰好是 240 帧。全量 Node、实际可见 fragment、GPU 上传和 draw 数与退出码共同检查。性能差异小于进程波动时只报告不确定，不宣称加速。

## Risks / Trade-offs

- [离屏跳过可能漏掉重新进入视口的文字] → 本轮不新增 Input 离屏跳过；仍用离开、进入、分数滚动和 resize 回归验证最新内容与 phase。
- [缓存错误绕过必要的布局或 clip 更新] → 按内容、布局、placement、scroll、clip revision 明确跳过条件，并对照完整同步路径。
- [计时本身增加开销] → 前后均保留相同 telemetry；记录子阶段总和与外层差值。
- [scroll translation 与旧 viewport origin 重复相加] → 以原始 Node bounds 为 origin，只保留一个世界平移来源；水平 scroll offset 仍按既有物理像素对齐。

## Migration Plan

先提交规划，后提交诊断 telemetry 与旧实测；根据结果更新设计并提交具体优化、回归及 Release 复测；最后运行正式 Windows MSVC 构建、相关 CTest 与 OpenSpec 校验。每个可独立验证阶段使用英文规范前缀提交。

本机 OpenSpec 1.4.1 的 `new change` / `status --change` 不接受以数字开头的仓库规定名称，故先用 CLI 创建字母名称脚手架，再在仓库内移到编号路径；`validate 025-20260925-optimize-gallery-scroll-sync --strict --no-interactive` 可用且通过。该版本没有 `doctor` 子命令。
