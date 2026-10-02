# 箭头合同

2026-10-02，Windows/MSVC、Ninja Multi-Config，平台通用 headless Debug/Release。

- Tooltip / Slider / TextComponent / GlyphTextureUpload / Recording SceneBackend：两配置 focused build 成功，CTest 各 5/5，Debug 2.86 秒、Release 2.18 秒。
- 12 placements 的 pointAtCenter/固定角内距、中心位置兼容、tiny viewport clamp、非法 inset 拒绝；reactive 居中不重新测量 trigger。四向 glyph 真正经 FontRuntime rasterize，核对纵横尺寸、方向覆盖与至少 100 个完全不透明像素。
- 关闭箭头减少一个 drawn glyph；纯背景/文字颜色保持 popup measure 和 surface geometry 不变，identity 与 idle 合同仍通过。私有箭头作为独立 window layer 的 Text 保留，删除 32 条 quad 近似，不新增 GPU ABI 或组件上传。
- `python tools/verify_icon_assets.py` 通过：原有 14 个 Ant source、许可、索引不变；新增四个 RynUI primitive 使用 U+F000–F003，manifest 区分来源和几何。
- fonttools 4.60.1、`PYTHONPATH=out/icon-tools` 的 `generate_icon_assets.py --check` 完全重现 checked-in CFF/manifest。
- clang-format 22.1.3 检查 395 个文件、0 failures；doctor healthy、full strict 43/43、git diff --check 通过。

共用 FreeType/R8/logical scene 合同通过不替代 GPU 图像验收；043 Windows 四向/多 DPI 与 Linux native 仍独立待验。
