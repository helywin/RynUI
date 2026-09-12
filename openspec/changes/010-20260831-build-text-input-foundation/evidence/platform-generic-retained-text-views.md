# 平台通用：独立文字显示层与滚动位移

任务 6.3 的基础子阶段；本记录不表示 Input 固定绘制层全部完成，6.3 保持未勾选。

- Windows MSVC，`windows-msvc-debug`：构建成功；`input_display|input_component|text_scene_service|text_component` 筛选的 5 项 CTest 全部通过。
- `TextSceneService::create_view` 创建独立的 retained glyph range、材质、clip 和 placement；共享源 `TextState` 的 shaping 与 measurement。源更新只触发一次 shaping；源销毁及 generation 复用不会替换已有 view 的共享状态。
- view 不允许修改共享内容、字体、约束或获取可变 `TextState`；自身颜色与透明度独立。
- `set_scroll_translation` 在 rasterization 后修改实例位移，保留 atlas UV、字形位置、clip 与 raster phase；调用方负责物理像素对齐。
- 测试执行 20,000 次滚动同步：glyph range、ordered scene rebuild 次数、rasterization 次数、shape 次数不变；clip 更新与源内容替换保留滚动位移，非有限位移被拒绝。
- 这里没有零堆分配计数，也没有真实窗口、DPI 或 GPU 画面验收；相应任务仍需后续证据。
