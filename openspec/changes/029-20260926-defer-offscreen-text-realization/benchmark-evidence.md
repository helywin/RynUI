# 离屏文本实现化基线与验收

规划基线复用 028 的 `../028-20260925-coalesce-gallery-atlas-transfers/gallery-texture-chunk-after.csv`：Windows 11 10.0.26200 / Intel Core Ultra 9 285HX，正式 `windows-msvc` / Ninja Multi-Config / MSVC Release，五个独立进程实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。首帧 CPU 中位数 189,322 µs、首帧资源同步 5,294 µs、首帧 glyph 纹理区域 1,077；各进程固定 240 步滚动成功，后续帧平均 CPU 中位数 4,156 µs。这些是代码变更前的测量，不是本 change 的效果。

规划校验：`openspec validate 029-20260926-defer-offscreen-text-realization --strict --no-interactive` 通过；全仓 strict 为 23/29，旧 change 013、015、016、017、018、021 失败；`openspec doctor --json` 在当前 CLI 报 `unknown command 'doctor'`；`git diff --check` 通过。

平台通用回归在 Windows MSVC Debug 运行：`rynui.text_component`、`rynui.text_component_frame`、`rynui.token_gallery_frame` 3/3 通过。新增测试确认首次布局后离屏文本已测量但无 glyph primitive；离屏期间修改内容，随后平移进入 clip，首个可见同步产生最新内容的 glyph 与当前 translation。Gallery 的长距离跳转测试确认新进入文本才生成 instance，已有文本不重建，已离屏的旧文本不做无关 geometry 更新。此阶段尚未测量真实窗口性能。

Windows 真实窗口复测：代码 `6798a7e` 在同一机器以正式 `windows-msvc` / Ninja Multi-Config / MSVC Release 完整 `--clean-first` 构建。五个独立进程运行 `--scroll-acceptance`，实际 SDL D3D12 / DXIL、1280×900 logical、display scale 1.25，均完成 240 步且退出码 0。新原始字段见 `gallery-offscreen-after.csv`；完整输出在被忽略的 `out/build/windows-msvc/offscreen-text-after-*.txt`。旧值直接复用 028 的五进程 `gallery-texture-chunk-after.csv`，不重复运行旧代码。

| 指标 | 028 旧基线中位数 | 029 新结果中位数 |
| --- | ---: | ---: |
| 首帧 CPU | 189,322 µs | 53,116 µs |
| 首帧资源同步 CPU | 5,294 µs | 3,511 µs |
| 首帧 glyph 纹理上传区域 | 1,077 | 257 |
| 首帧纹理 transfer 创建 | 5 | 2 |
| 首帧 GPU 上传提交 | 1 | 1 |
| 240 帧滚动平均 CPU | 4,156 µs | 4,205 µs |
| 240 帧滚动 p95 CPU | 4,350 µs | 4,583 µs |
| 240 帧滚动最长帧 CPU | 4,796 µs | 15,199 µs |
| 滚动期间新增字体 raster | 0 | 740 |

首帧 CPU 中位数减少约 72%，五次旧/新取值区间不重叠。代价是原来首帧完成的部分字形工作转移到滚动：滚动平均增加约 49 µs，p95 增加约 5.4%，最长帧增加到约 15 ms。不能把首帧收益描述为整体无回归；滚动期间 740 次新增 raster 是下一阶段需要关注的停顿来源。五次文档最终 offset 均为最大值 15,068、section 为 `gallery.document.live-samples`，可见 fragment 数仍为 87。累计 draw / 提交数因启动速度和滚动期间资源实现化而变化，不能仅按生命周期计数判断像素等价；逐帧像素与 GPU 执行时间未测。

Release 真实 D3D12 `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均退出码 0。另做一次半视口 overscan 的探索性运行，首帧上传 364 个区域且滚动最长帧约 22.8 ms，未保留该试验改动；恢复正式 32 logical px guard 后再次构建并通过滚动验收，首帧上传恢复为 257 个区域。

集成复核：Windows MSVC Release 完整 CTest 为 229/235，通过数与 028 一致。失败项仍是 `rynui.design_token_catalog`、`rynui.ant_design_current_baseline`、`rynui.ant_design_665_evidence_contract`、`rynui.ant_design_gallery_catalog_generator`、`rynui.theme_algorithm`、`rynui.dependency_lock`；完整日志保存在被忽略的 `out/build/windows-msvc/offscreen-text-full-ctest-msvc.log`。本 change strict validate 通过；全仓 strict 为 23/29，旧 change 013、015、016、017、018、021 失败。当前 OpenSpec CLI 对 `openspec doctor --json` 报 `unknown command 'doctor'`。`git diff --check` 通过。由于完整 CTest 与全仓 strict 未通过，`tasks.md` 4.1 保持未勾选。
