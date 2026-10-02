# 完整离线目录合同

2026-10-03，Windows / MSVC / Ninja Multi-Config，windows-msvc-headless Debug/Release。

- 锁定 tgz SHA256 7e07fdcf459f1f2ae6721ca1796811d9fb7737693af0724b55840f4edccde80b；import_icon_assets.py --check 对全部 848 SVG 与原归档逐字节比较。
- generate_icon_assets.py --check（FontTools 4.60.1）重现字体/公开名称/层表/manifest，verify_icon_assets.py 验证许可、名称、codepoints、层与生成 font hash。
- 实际 FreeType glyph lookup + raster：848 icons / outlined 447 / filled 251 / two_tone 150 / 1052 layers / 最大四层；每层都有非零 coverage，四个 Tooltip primitives 没有碰撞。
- HEADLESS 受影响六项 CTest：Debug 6/6（3.48 秒）、Release 6/6（2.43 秒），包含 icon_catalog / icon_asset_contract / text_component / tooltip_component / password_component / search_component。
- clang-format 22.1.3 + 自有源码格式门、OpenSpec doctor/51 个 strict change 校验及 diff 检查通过。

本阶段仅完成资源与目录合同；双色呈现、旋转/spin、自定义向量仍由后续任务实现，未把首层 glyph 显示描述为完整双色渲染。
