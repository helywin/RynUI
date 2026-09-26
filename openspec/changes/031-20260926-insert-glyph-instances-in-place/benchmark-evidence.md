# Glyph 实例插入基线与验收

旧基线复用 030 的 `../030-20260926-batch-text-scene-order-rebuild/gallery-order-batch-after.csv`：同一 Windows 11 / Intel Core Ultra 9 285HX、正式 MSVC Release、五个独立 D3D12 Gallery 进程，首帧 CPU 中位数 53,592 µs，滚动平均 4,206 µs、p95 4,544 µs、最长 15,227 µs；每次固定 240 步，最终 offset 15,068。030 的临时分段诊断单次测得最后跳转帧 `GlyphScene::replace_text` 约 11.5 ms，ordered scene 重建约 11 µs、range remap 约 14 µs；诊断代码已撤销。旧版本每个零长度插入都会创建恰好大小的完整新 vector，此为源码事实，真实收益仍须本 change 复测。

规划校验：本 change strict validate 通过；全仓 strict 为 25/31，旧 change 013、015、016、017、018、021 失败。当前 CLI 的 `openspec doctor --json` 报 `unknown command 'doctor'`；`git diff --check` 通过。

平台通用回归在 Windows MSVC Debug 定向构建后运行：`rynui.glyph_scene`、`rynui.text_scene_service`、`rynui.token_gallery_frame` 3/3 通过。新增测试覆盖零长度中间插入后的精确顺序、geometry/material dirty 后缀、与 store 重叠的源 span，以及 128 次小插入时底层 data 指针变化少于 32 次。快路径先预留 dirty range 容量，实例插入成功后再写 dirty；普通非零长度替换保留原实现。此阶段尚未报告真实窗口性能收益。

Windows 真实窗口复测：代码 `4189317` 在同一机器以正式 `windows-msvc` / Ninja Multi-Config / MSVC Release 完整 `--clean-first` 构建。五个独立进程运行 `--scroll-acceptance`，实际 SDL D3D12 / DXIL、1280×900 logical、display scale 1.25，均完成固定 240 步且退出码 0。新原始字段见 `gallery-glyph-insert-after.csv`，完整输出在被忽略的 `out/build/windows-msvc/glyph-insert-after-*.txt`；旧值直接复用 030 的五进程 CSV。

| 指标 | 030 旧基线中位数 | 031 新结果中位数 |
| --- | ---: | ---: |
| 首帧 CPU | 53,592 µs | 53,670 µs |
| 滚动平均 CPU | 4,206 µs | 4,160 µs |
| 滚动 p95 CPU | 4,544 µs | 4,543 µs |
| 滚动最长帧 CPU | 15,227 µs | 4,806 µs |
| 滚动平均 scene sync CPU | 504 µs | 206 µs |
| 首帧 glyph 上传区域 / 滚动新增 raster | 257 / 740 | 257 / 740 |
| 滚动 buffer 上传区域 / transfer 创建 | 542 / 419 | 542 / 419 |

最长 CPU 帧中位数减少约 68%，五次旧区间 14,809–15,974 µs 与新区间 4,758–5,300 µs 不重叠。滚动平均 scene sync 明显下降；首帧与 p95 接近旧值，不能声称这些指标进一步加速。五次最终 offset 均为最大值 15,068、section 为 `gallery.document.live-samples`，滚动工作量和 glyph raster/上传计数保持一致。累计 draw/提交数随运行帧数变化，不能代替逐帧像素比较；GPU 执行时间仍未测。Release 真实 D3D12 `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均退出码 0。

集成复核：正式 Windows MSVC Release 完整构建通过；完整 CTest 为 229/235，通过率 97%，失败项仍为此前已有的 `rynui.design_token_catalog`、`rynui.ant_design_current_baseline`、`rynui.ant_design_665_evidence_contract`、`rynui.ant_design_gallery_catalog_generator`、`rynui.theme_algorithm`、`rynui.dependency_lock`，本 change 的测试均通过。本 change strict validate 通过；全仓 strict 仍为 25/31，失败的是既有 change 013、015、016、017、018、021。当前 OpenSpec CLI 的 `doctor --json` 不可用，报 `unknown command 'doctor'`；`git diff --check` 通过。由于完整 CTest 和全仓 strict 门槛尚未通过，tasks 4.1 保持未勾选。完整测试输出保存在被忽略的 `out/build/windows-msvc/glyph-insert-full-ctest-msvc.log`。
