# 文本绘制顺序批次基线与验收

旧基线复用 029 的 `../029-20260926-defer-offscreen-text-realization/gallery-offscreen-after.csv`：同一 Windows 11 / Intel Core Ultra 9 285HX 的五个独立 Release D3D12 Gallery 进程，首帧 CPU 中位数 53,116 µs，滚动 240 帧平均 4,205 µs、p95 4,583 µs、最长 15,199 µs；最后 offset 为 15,068、section 为 `gallery.document.live-samples`，滚动期间新增 raster 为 740。030 改代码前的临时诊断单次测得：第 240 步跳转到 offset 15,068，最长帧约 15.4 ms，其中宿主布局/同步约 12.3 ms、文本 mounted loop 约 12.0 ms、文本场景同步约 12.0 ms、资源同步约 2.4 ms，新增 raster 55。诊断仅定位 CPU 阶段，未测 GPU 执行时间；临时日志保存在被忽略的 `out/build/windows-msvc/scroll-peak-phase2.txt`，诊断代码已移除。

规划校验：本 change strict validate 通过；全仓 strict 为 24/30，旧 change 013、015、016、017、018、021 失败。当前 CLI 的 `openspec doctor --json` 报 `unknown command 'doctor'`；`git diff --check` 通过。诊断临时代码撤销后的 Gallery 文件已恢复为提交内容，未混入本 change 规划提交。

平台通用回归在 Windows MSVC Debug `--clean-first` 定向构建后运行：`rynui.text_scene_service`、`rynui.text_component`、`rynui.text_component_frame`、`rynui.token_gallery_frame` 4/4 通过。新增测试将三个不同长度文本的批次输出与逐条同步的 draw commands、instance ranges 比较；批次内不重建、结束只重建一次、无变更批次不重建，取消后下一次普通同步可恢复。Gallery 的长距离跳转仍实现化新文本，但同轮 ordered scene 仅重建一次。此阶段尚未报告真实窗口性能收益。
