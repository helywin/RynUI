# Design

## Context

见 proposal。现有 Card/Tag 保留 typed slot、Surface 和 Theme identity，Icon 支持 vector source 和有界动画周期；输入已有独立于绘制活动的 component_active 检查。

## Goals / Non-Goals

提供紧凑的反馈 API、稳定内容身份和可停止的加载反馈。仅扩展局部服务和 Theme 字段；不新增 renderer/scene ABI、通用样式入口或另一套 Status。

## Decisions

- 公共 API 使用独立 AlertProps/SpinProps/ProgressProps 和 typed slots。Alert 使用 Tag 的 Status（None/Default 为 info，Processing 为 info）；Progress 使用专用 ProgressStatus Normal/Success/Exception，避免假装支持 active shimmer。
- Alert close 只请求 onClose，visible 由调用者写回；message/description 默认文本与可选 slot 共用语义字体，action/close 始终保留。
- Spin 在内容后绘制 Theme dim mask 和 indicator，内容不失活、不重建；给 InteractionRegistry 增加有生命周期的 blocked subtree 记录，沿用 input boundary revision 的捕获/焦点/编辑会话清理。避免篡改子控件 disabled/eligible 值，支持嵌套和动态内容。
- Spin 默认 LoadingOutlined，复用 Icon 的 finite AnimationRuntime 旋转周期；离屏、祖先隐藏、window inactive、spinning=false、reduced/Theme motion=false 撤销 deadline。无内容可独立使用。
- Progress line 用 rounded fill；circle/dashboard 用固定数量的 round dots 构成圆弧，共同 CPU RoundedEffect store 保持稳定 topology。零模糊 fill 复用已有祖先裁剪合同，避免扩大 Surface 的 16 层上限。percent clamp 到 0..100，非有限值拒绝；100 默认完成态，Exception 优先。代价是圆弧为有限采样，后续 GPU 人工视觉单独验证。
- 三组 typed component token 解析 Default/Dark/Compact、nested override/algorithm、identity serialization 和按颜色/metrics 分开的精确失效；扩展 golden 时比较全部旧字段（identity 除外）。

## Risks / Trade-offs

- 输入屏蔽重入 → 使用现有 revision reconciliation 与 generation 身份，测试焦点、capture、键盘、动态子节点与嵌套。
- 无限忙循环 → 每周期有限、结束时仅在仍可见和活动时重排；controlled clock 测试 idle、离屏和销毁。
- 视觉精度 → 固定 rounded primitives，记录未做真实 GPU/DPI 视觉验收，不扩大 renderer 合同。

## Migration Plan

新增 API，无旧调用迁移。独立工作树 `windows-msvc-headless` preset、Ninja Multi-Config/MSVC，Debug/Release 定向 CTest；本机 helper 和构建目录仅在忽略的 out 下，最多 4 compile jobs。分阶段英文本地提交；根据用户最新策略，不再推送工作树分支，由父任务合并、验证并推送 main。规划提交曾在策略变更前推送，可逐提交回滚。
