# 文本绘制顺序批次基线与验收

旧基线复用 029 的 `../029-20260926-defer-offscreen-text-realization/gallery-offscreen-after.csv`：同一 Windows 11 / Intel Core Ultra 9 285HX 的五个独立 Release D3D12 Gallery 进程，首帧 CPU 中位数 53,116 µs，滚动 240 帧平均 4,205 µs、p95 4,583 µs、最长 15,199 µs；最后 offset 为 15,068、section 为 `gallery.document.live-samples`，滚动期间新增 raster 为 740。030 改代码前的临时诊断单次测得：第 240 步跳转到 offset 15,068，最长帧约 15.4 ms，其中宿主布局/同步约 12.3 ms、文本 mounted loop 约 12.0 ms、文本场景同步约 12.0 ms、资源同步约 2.4 ms，新增 raster 55。诊断仅定位 CPU 阶段，未测 GPU 执行时间；临时日志保存在被忽略的 `out/build/windows-msvc/scroll-peak-phase2.txt`，诊断代码已移除。

规划校验：本 change strict validate 通过；全仓 strict 为 24/30，旧 change 013、015、016、017、018、021 失败。当前 CLI 的 `openspec doctor --json` 报 `unknown command 'doctor'`；`git diff --check` 通过。诊断临时代码撤销后的 Gallery 文件已恢复为提交内容，未混入本 change 规划提交。

平台通用回归在 Windows MSVC Debug `--clean-first` 定向构建后运行：`rynui.text_scene_service`、`rynui.text_component`、`rynui.text_component_frame`、`rynui.token_gallery_frame` 4/4 通过。新增测试将三个不同长度文本的批次输出与逐条同步的 draw commands、instance ranges 比较；批次内不重建、结束只重建一次、无变更批次不重建，取消后下一次普通同步可恢复。Gallery 的长距离跳转仍实现化新文本，但同轮 ordered scene 仅重建一次。此阶段尚未报告真实窗口性能收益。

Windows 真实窗口复测：代码 `53d68b1` 在同一机器以正式 `windows-msvc` / Ninja Multi-Config / MSVC Release 完整 `--clean-first` 构建。五个独立进程运行 `--scroll-acceptance`，实际 SDL D3D12 / DXIL、1280×900 logical、display scale 1.25，均完成固定 240 步且退出码 0。新字段见 `gallery-order-batch-after.csv`，完整输出在被忽略的 `out/build/windows-msvc/text-order-batch-after-*.txt`。旧值复用 029 的五进程 CSV。

| 指标 | 029 旧基线中位数 | 030 新结果中位数 |
| --- | ---: | ---: |
| 首帧 CPU | 53,116 µs | 53,592 µs |
| 滚动平均 CPU | 4,205 µs | 4,206 µs |
| 滚动 p95 CPU | 4,583 µs | 4,544 µs |
| 滚动最长帧 CPU | 15,199 µs | 15,227 µs |
| 累计 ordered scene 重建 | 582 | 220 |
| 首帧 glyph 上传区域 / 滚动新增 raster | 257 / 740 | 257 / 740 |

重建次数减少 362 次，但首帧、滚动平均和最长帧都处于旧/新五进程重叠区间，不能宣称用户可见的长帧已改善。五次最终 offset 均为 15,068、section 为 `gallery.document.live-samples`，可见 fragment 数仍为 87；累计 draw 数会随启动时序变化，不能代替逐帧像素比较。GPU 执行时间未测。Release 干净重建后另一次滚动验收与 `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均退出码 0。

为定位剩余峰值，临时在 `TextSceneService` 分段计时并复测一次第 240 步：约 14.3 ms CPU 帧中，`GlyphScene::replace_text` 约 11.5 ms、后续 range remap 约 14 µs、ordered scene 重建约 11 µs。该单次诊断不作为五进程性能结论，临时代码已撤销，日志在被忽略的 `out/build/windows-msvc/text-order-followup-profile.txt`。因此下一轮应处理 glyph instance 插入时的整数组复制，而不是继续优化 ordered scene 重建。
