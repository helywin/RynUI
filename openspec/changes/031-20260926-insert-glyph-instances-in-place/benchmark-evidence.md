# Glyph 实例插入基线与验收

旧基线复用 030 的 `../030-20260926-batch-text-scene-order-rebuild/gallery-order-batch-after.csv`：同一 Windows 11 / Intel Core Ultra 9 285HX、正式 MSVC Release、五个独立 D3D12 Gallery 进程，首帧 CPU 中位数 53,592 µs，滚动平均 4,206 µs、p95 4,544 µs、最长 15,227 µs；每次固定 240 步，最终 offset 15,068。030 的临时分段诊断单次测得最后跳转帧 `GlyphScene::replace_text` 约 11.5 ms，ordered scene 重建约 11 µs、range remap 约 14 µs；诊断代码已撤销。旧版本每个零长度插入都会创建恰好大小的完整新 vector，此为源码事实，真实收益仍须本 change 复测。

规划校验：本 change strict validate 通过；全仓 strict 为 25/31，旧 change 013、015、016、017、018、021 失败。当前 CLI 的 `openspec doctor --json` 报 `unknown command 'doctor'`；`git diff --check` 通过。

平台通用回归在 Windows MSVC Debug 定向构建后运行：`rynui.glyph_scene`、`rynui.text_scene_service`、`rynui.token_gallery_frame` 3/3 通过。新增测试覆盖零长度中间插入后的精确顺序、geometry/material dirty 后缀、与 store 重叠的源 span，以及 128 次小插入时底层 data 指针变化少于 32 次。快路径先预留 dirty range 容量，实例插入成功后再写 dirty；普通非零长度替换保留原实现。此阶段尚未报告真实窗口性能收益。
