# 082 Gallery 实施证据

## 基线与边界

- 起点 `62c990a1b494532c358ac487b2bb5931c08a7231`，工作树干净；显式 merge `7db4123842d9815adef1c86ce32e8a003c24e827`，保留双方历史。
- 只追加导航 Gallery、生产 anchor helper、frame fixture 与目标文档。未改 Core、公开头、Theme、CMake ABI、golden；不导入 Time/Descriptions。
- 规划使用全局 OpenSpec 1.14.0 的 context/list/status/instructions；doctor healthy、strict validation 81/81、diff check 通过，规划提交 `c8c9ca7`。用户已明确授权规划后连续 apply。

## 构建与输入验证

Windows MSVC 19.51.36256.0，`windows-msvc` configure，Ninja Multi-Config；所有 build 使用 `--parallel 4`。Recording fixture 使用锁定字体与实际组件运行时，不是原生窗口或 GPU 验收。

```text
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug --target rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests --parallel 4
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --navigation-only
```

导航 fixture 通过样例 heading 所有权定位 Breadcrumb、Steps、Input、Button、编辑器和场景，使用真实目录 Pointer 点击与生产 anchor/jump；文本通过 TextCommitted、Ctrl+A、Home/Shift+Right 选区事件输入。Steps 使用真实方向键/Home/End 焦点移动、Enter/Space 提交与受控回写。检查 glyph、quad、hit clip、外扩焦点四边 coverage 及组件/editor/scene/interaction 身份；公开 InputRef 由样例外部编辑按钮实际调用。

样例标题按现有 Form/Date 留出 1dp 字体墨迹空间。显式 disabled 项通过真实 HitTest/Focus 拒绝检查，未要求 branch gate 改写子 Input 自身 disabled 属性。

`--navigation-only` 六组组合全部退出 0：Default（smoke step 4）、Dark（0）、Compact（1），分别 1200×900 / 560×900。补强原生 editor glyph、逐项而非整组 allocation clip、隐藏 glyph 移除与禁用子按钮断言后，六组再次全部通过。旧 `--forms-only` 与 `--panels-only` 各实际运行一次、退出 0，保留原 `1.`、`1.0000004`、`1.234567891234` 及 Collapse body-owned Select 断言。

`--date-picker-only` 最终退出 0，包含原三主题双宽矩阵和 same-value String reset-before-blur 断言。追加内容取消旧页尾 scroll clamp 后，旧夹具把 reset 滚到打开日历下面，导致实际点击未到按钮；夹具现将自有按钮放在视口绝对 y=780，给上方翻转的日历留空间，并新增实际 glyph 与 HitTest 命中按钮断言。中间一次相对坐标误用令按钮落在 y=880，已修正为绝对坐标；没有放宽 reset/draft/error/preedit 或 retained 断言，也未修改 DatePicker Core。

```text
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --panels-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
```

## 初始盘点

`--inventory-only` 在 1200×60000 下只用于盘点，不能作为可达性证据：

```text
stable_ids=319 navigation=93 buttons=337 interactions=1072 visible_interactions=634 live_samples=284 theme_content=67 inputs=144 selects=22 modal_drawer=13 selection=46 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=2 form_items=5 date_pickers=2 breadcrumbs=2 steps=1
```

旧 297 个 ID 保持原顺序；实际旧 `add_*` 函数为 28 个，函数体逐字相同、声明与调用顺序相同。`compare-baseline.py` 复核这些边界及全部 71 个非 Breadcrumb/Steps overlay 条目。

## 验收边界

完整 frame/native 矩阵由父线程统一执行，本 worker 不重复。固定拓扑、无 RTL/router、复杂 Steps 变体未覆盖，仍为 partial；不声明 GPU、原生窗口、Linux、系统 IME 或辅助技术通过。父线程还需新一轮 Astra 独立审查。
