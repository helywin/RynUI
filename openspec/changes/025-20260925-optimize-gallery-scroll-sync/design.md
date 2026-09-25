# Design

## Context

024 的五进程实测显示 `frame_layout_us` 约 14 ms，但它包括 `TextComponentHost::layout_and_synchronize`、参与者 geometry/fragment、effect compact、文本 fragment 和 scene composer；命中刷新仅约 5 µs。Gallery 每次滚动仍遍历文档子树写入 translation，累积访问 222,443 个 Node。现有外层计时不足以判断哪段首先值得优化。

## Goals / Non-Goals

**Goals:** 将同步外层拆成互斥子阶段，并记录 mounted text 访问、实际同步、离屏跳过、geometry patch/rebuild 与 participant 工作量；使用相同真实窗口场景比较五进程中位数。选择贡献最大且能保持现有语义的重复工作，先建立失败与等价回归，再改实现。

**Non-Goals:** 不把 CPU 阶段时长当作 GPU 时间；不重写滚动坐标体系、公开组件接口或 GPU renderer。

## Decisions

1. **先观测再改算法。** 在原有 `--scroll-acceptance` 输出中增加命名稳定的累积时间与计数；每个阶段以同一帧数为分母，明确计时口径。先运行五个独立 Release D3D12 进程，并保存原始数值。
2. **根据实测更新实施设计。** 只有明确最大子阶段和造成它的重复工作后，才将具体跳过条件、失效规则与测试 oracle 写入本文件并进入改造。不得凭 024 的外层 14 ms 直接删去同步工作。
3. **前后比较。** 保持机器、preset、窗口、自动滚动步骤及 telemetry 一致。全量 Node、实际可见 fragment、GPU 上传和 draw 数与退出码共同检查。性能差异小于进程波动时只报告不确定，不宣称加速。

## Risks / Trade-offs

- [离屏跳过可能漏掉重新进入视口的文字] → 用离开、进入、分数滚动和 resize 回归验证最新内容与 phase。
- [缓存错误绕过必要的布局或 clip 更新] → 按内容、布局、placement、scroll、clip revision 明确跳过条件，并对照完整同步路径。
- [计时本身增加开销] → 前后均保留相同 telemetry；记录子阶段总和与外层差值。

## Migration Plan

先提交规划，后提交诊断 telemetry 与旧实测；根据结果更新设计并提交具体优化、回归及 Release 复测；最后运行正式 Windows MSVC 构建、相关 CTest 与 OpenSpec 校验。每个可独立验证阶段使用英文规范前缀提交。

本机 OpenSpec 1.4.1 的 `new change` / `status --change` 不接受以数字开头的仓库规定名称，故先用 CLI 创建字母名称脚手架，再在仓库内移到编号路径；`validate 025-20260925-optimize-gallery-scroll-sync --strict --no-interactive` 可用且通过。该版本没有 `doctor` 子命令。
