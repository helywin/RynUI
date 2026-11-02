# 104 验收结果

## Windows 系统字体阶段

- 实际平台：Windows / MSVC / Ninja Multi-Config，`windows-msvc-release`。
- 相关目标 `rynui_default_font_chain_tests` 构建成功；`ctest --preset windows-msvc-release -R '^rynui\.default_font_chain$' --output-on-failure` **1/1 通过**。
- 新 emoji 回归对原链实际失败；补入系统 Symbol/Emoji 后，系统字体发现、真实塑形和灰度 bitmap 覆盖通过。普通启动不依赖字体文件；包含 style、mono、尺寸及 DPI 的链复用检查。
- 此阶段只认可字体候选变更。其他视觉/输入修复仍在验证；不代表 Gallery 整页、Linux、彩色 emoji、系统 IME 或 AT 验收。

原始执行记录只留本机已忽略的 `out/`，不作为仓库附件。
