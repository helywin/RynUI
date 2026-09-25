# 离屏文本实现化基线与验收

规划基线复用 028 的 `../028-20260925-coalesce-gallery-atlas-transfers/gallery-texture-chunk-after.csv`：Windows 11 10.0.26200 / Intel Core Ultra 9 285HX，正式 `windows-msvc` / Ninja Multi-Config / MSVC Release，五个独立进程实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。首帧 CPU 中位数 189,322 µs、首帧资源同步 5,294 µs、首帧 glyph 纹理区域 1,077；各进程固定 240 步滚动成功，后续帧平均 CPU 中位数 4,156 µs。这些是代码变更前的测量，不是本 change 的效果。

规划校验：`openspec validate 029-20260926-defer-offscreen-text-realization --strict --no-interactive` 通过；全仓 strict 为 23/29，旧 change 013、015、016、017、018、021 失败；`openspec doctor --json` 在当前 CLI 报 `unknown command 'doctor'`；`git diff --check` 通过。

平台通用回归在 Windows MSVC Debug 运行：`rynui.text_component`、`rynui.text_component_frame`、`rynui.token_gallery_frame` 3/3 通过。新增测试确认首次布局后离屏文本已测量但无 glyph primitive；离屏期间修改内容，随后平移进入 clip，首个可见同步产生最新内容的 glyph 与当前 translation。Gallery 的长距离跳转测试确认新进入文本才生成 instance，已有文本不重建，已离屏的旧文本不做无关 geometry 更新。此阶段尚未测量真实窗口性能。
