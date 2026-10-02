# 真实窗口发现的共享可见性回归

052 原生窗口/GPU 读回发现两项平台通用实现缺口，已补独立回归。

1. clearDisabled 从 true 恢复 false 时，仅更新 InteractionRegistry，没有将 HitTest 放入 dirty 队列；若没有同时变更图标/尺寸，旧 disabled 快照阻止点击。InputAffixAction 在资格实际变化时显式 invalidate HitTest，仍保持禁用时取消捕获。input_actions 现在验证无几何变化的重新命中。
2. showCount=false 已清空 CPU 标签和布局尺寸，但 TextComponent 的零尺寸/offscreen 跳过路径留下旧 glyph coverage。TextSceneService::synchronize_culled 在宿主同步内 patch 为零 clip，不 shape/rasterize、不改变 range owner；重新可见时恢复 clip 并应用 pending 内容/颜色/旋转。Icon 各层及 Typography 装饰沿用同一裁剪路径。input_count 验证隐藏后的 glyph clip，text_scene_service 验证重复 cull、缓存、material/rotation 恢复和 pending 空值。

Gallery 滚动合同现在允许无 inherited clip 的 glyph geometry 更新仅用于将 coverage 完全裁剪，继续禁止重建已实现的字形几何。此前测量/CPU 标签断言不能代替 GPU 可见性，专用窗口脚本同时检查状态截图 hash 不同。

修复后的 windows-msvc-headless 完整 CTest：Debug 85/85（119.52 s），Release 85/85（23.29 s）。input_scene_allocation 保持 selection/composition-selection 零分配合同。windows-msvc 受影响 20 CTest：Debug 20/20（38.04 s），Release 20/20（14.89 s）；其中平台字体合同与真实窗口证据另见 Windows README。

同一原生检查还发现系统字体缺少 U+FFFD：默认字体链现在按需追加显式 bundled Latin fallback，并保留共享字体文件的 UI/monospace 顺序及元数据。此项系统字体行为在 Windows 实测，Linux 保持独立待验收。
