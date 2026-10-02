# 共同 retained glyph 旋转

2026-10-03，Windows / MSVC / Ninja Multi-Config。

- windows-msvc-headless Debug/Release：logical_scene_packing、glyph_scene、text_scene_service、scene_backend、glyph_gpu_resources、text_component、icon_catalog 共 7/7（1.69 / 1.31 秒）。
- Literal 90 度结果与三种非正方形/正方形 viewport、不同 density 的四个角对照 logical/packed reference；0/45/90/-30/360/大角度，UV/opacity/material 保留，NaN/Inf 拒绝。旧 ABI v1 被 preflight 拒绝且不创建资源。
- 连续 24 次 TextScene transform：range/shape_count/rasterizations/texture uploads 不增加，geometry 独立更新；placement/content rebuild 保留 local pivot/角度，重复值不请求空帧，销毁后 scene/instances 为零。
- windows-msvc Debug/Release：实际 DXIL/SPIRV glyph VS/PS 编译与 reflection 通过；SDL attributes 为 0/16/32/48/64/80，96-byte stride。原生三项 packing/glyph/text scene 回归各 3/3（0.69 / 0.54 秒）。
- 实际 D3D12/DXIL Gallery --smoke 两配置退出 0；这是旧图标/零旋转兼容 smoke，完整 Icon 双色/旋转/spin 与各比例 readback 属于 6.1。
- clang-format 22.1.3、格式门、doctor/51 changes strict 与 diff 检查通过；同步正式 renderer-contract、architecture 与 AGENTS 的 logical v3 / packed v2 版本引用。
