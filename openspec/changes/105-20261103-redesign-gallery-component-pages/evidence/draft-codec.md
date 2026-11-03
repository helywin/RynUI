# 草稿值编码子阶段

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 构建 `rynui_gallery_draft_codec_tests`：通过。定向 CTest `rynui.gallery_draft_codec`：1/1 通过。
- 覆盖 Unicode、混合选项、optional 空值、Select/Radio/Checkbox 值、Date/Time、Statistic 的最大 uint64、Result/Avatar variant、Slider range、Table sort 和空列表覆盖；非法 variant、整数溢出和无效日期被拒绝。
- 编码结果仍只包含 GalleryPageDraft 允许的基础值；没有缓存 Prop、Signal、ref、回调或组件/编辑器/renderer identity。

这是 3.5 的独立基础子阶段，尚未把所有其余组件字段与页面迁移接完，3.5 保持未完成。没有运行窗口或扩大 CTest 集合；原始输出仅在 `out/105-implementation/20261103/`。
