# 073 平台通用集成收口

2026-10-24，Windows 实机 MSVC headless 验证。原始实现及其成功/失败证据仍保留在 [首次验收](../platform-common-msvc-headless.md)，本记录补充共享主线修复合入后的实际结果。仅本地提交，没有 fetch、push、PR、archive 或子任务。

## 基线与源码范围

- 工作树：`C:\Users\jiang\.t3\worktrees\RynUI\t3-2b08410a`；分支：`t3/native-empty-result-components`。
- 原始 073：`a6017b1bcb10f38a551491302fc639652071afde`。
- 合入的 committed main：`c3190b00210112a3a1ccf3a350cf0290082a4b14`；合并提交：`bc0a277f3878677738361cecaa024c396e652bca`。
- Popconfirm 最小空集合 fast path：`c3b84839029d1d292dc2c549c6c72efb1c2cf33f`。只在 `position_window_layers` 的 snapshot copy 前检查空集合，非空 copy 与重入生命周期语义保持。
- Astra R1 两项 P2 的修复提交、最终完整测试源代码：`28bee9e744eedf142ceca8c2cf63311346d5cba9`。复现、修复和专项日志见 [review-r1.md](review-r1.md)。

`source-checks.py` 从真实 Git baseline 与工作树逐块比较：主线 24 个 Theme resolution lambda、073 两个 lambda、Table/Menu/InputNumber/Collapse/Segmented 实现、主线 host 顺序和 wheel routing 保持；Popconfirm 仅增加空集合 guard；073 API 与 Theme 测试保持原始版本，组件源码/回归精确对应修复提交。合并自动带入的全局文档及 committed 074 planning 保持 c319 原样，未复制 root 未提交的 074 文件。父任务之后的 Gallery `9ecb1f5` 和 wheel 修复 `0de353b` 未在此工作树合入或重跑，由父任务后续集成保留。

## 实际构建和测试

官方 preset `windows-msvc-headless`，独立 `out/build/empty-result-headless`，`Ninja Multi-Config`，MSVC **19.51.36256.0**，`HEADLESS/RECORDING`，bundled dependencies；每次构建最多 **4 compiler jobs**。配置使用已有依赖 source cache，所有本次 object/link 产物位于独立 build tree。Debug `/Ob0 /Od /RTC1`，默认 `/debug /INCREMENTAL`，没有增大线程栈或关闭 iterator debug。

```text
cmake --preset windows-msvc-headless -B out/build/empty-result-headless -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_freetype-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_harfbuzz-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_sheenbidi-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_utf8proc-src
cmake --build out/build/empty-result-headless --config Debug --parallel 4
ctest --test-dir out/build/empty-result-headless -C Debug --output-on-failure -j 4
cmake --build out/build/empty-result-headless --config Release --parallel 4
ctest --test-dir out/build/empty-result-headless -C Release --output-on-failure -j 4
```

最终修复提交完整构建均成功；完整 Debug **148/148，217.00 秒**，Release **148/148，18.27 秒**。日志为 `full-debug.log`、`full-release.log`；机器可读汇总为 `results.json`。修复前集成基线 `c3b8483` 的完整 Debug **148/148，280.22 秒** 另存，未将其当作新增回归通过证据。

从实际 CTest JSON 枚举选择的原始失败专项名称是 `rynui.portable.button_component`、`rynui.portable.switch_features`、`rynui.portable.popconfirm_theme`、`rynui.portable.popconfirm_component`、`rynui.portable.input_scene_allocation`。空集合 guard 后 Debug **5/5，258.98 秒**、Release **5/5，13.62 秒**；原始 Button/Switch/Popconfirm Theme 的 Debug 栈失败已通过共享主线 IIFE 修复消除。

实际输入场景分配日志保存在 `allocation-debug.log` / `allocation-release.log`：256 个 Input，selection 与 composition-selection 各 **20,000** 次更新，两种模式在 Debug/Release 均为 **0 total / 0 dispatch / 0 sync allocations**。Debug 耗时分别 118721/138496 ms，Release 6373/6865 ms。最终修复后完整 Debug/Release 也通过此同名合同；R1 修复未改输入或 Popconfirm 热路径。没有发现第二处分配，因此未扩展优化范围。

## Golden、格式与范围

最终 Debug 全量构建后的 `tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 重新生成五份 golden，没有产生 Git 差异。`compare-goldens.py` 对 main c319 的 **13,152** 个旧 leaf 值及原始 a601 的 **12,857** 个旧 leaf 值逐项保持，只忽略顶层 identity；相对 main 仅新增 empty/result，相对原始 073 仅新增 collapse/segmented。每个文件的数量见 `results.json`。

clang-format **22.1.3** 检查 **583** 个自有源文件，0 failures；`openspec doctor --json` healthy，`openspec validate --all --strict --no-interactive` **74 passed / 0 failed**；`git diff --check` 通过。073 的 4.1 按本次实际全部成功结果收口。

本次仅完成平台通用静态展示：实际 node/text/glyph/surface、有限/短/零约束、reactive label/status/theme、保留 Button/Input action/editor 状态、margin 和零/极小图示尺寸、DPI 字体恢复。未运行 native Gallery。真实 GPU、Linux、系统 IME、屏幕阅读器桥接和上游大型 SVG 插画像素等价均未宣称验证；静态实现仍不提供动画、网络/服务、栅格资产或动态 slot 拓扑。这些边界不阻塞本 change 的平台通用完成。
