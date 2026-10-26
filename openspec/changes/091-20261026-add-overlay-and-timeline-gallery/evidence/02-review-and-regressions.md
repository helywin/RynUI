# 091 审查修复与旧流程回归

## 对象与 R1 修复

R1 固定审查实施提交 `2a5989faa51e704fe02c5c1fe060c82e7ba38749`，发现通知 actions 和默认正序 Timeline 末行的原生 Button 焦点装饰空间不足。两项都先加入真实 Input → Tab → 自身 Button 的四边 coverage 断言，再修正新增样例的外部布局，没有更改 Core/Theme/renderer，也没有放宽原 clip/coverage 断言。

- `out/091-main-r1-repro-focused.log`：Notification 阶段新增焦点断言失败，exit 1。
- 仅给通知 action 加四边 6 dp margin 后，`out/091-main-r1-notification-fixed-focused.log`：Notification 通过，默认正序 Timeline 阶段新增焦点断言失败，exit 1。
- 再给 Timeline save 加四边 6 dp margin 后，正式三目标实际新构建和 `--overlays-timeline-only` 全部退出 0。输出见 `out/091-main-r1-both-fixed-build.log` 和 `out/091-main-r1-both-fixed-focused.log`。

本轮还补齐 R1 指出的覆盖缺口：每个 Theme/宽度组合实际打开 Message、同 key 更新为 Success，检查实际 glyph、背景 fill 和 HitTest；与 Notification 同时存活时执行 clear，验证两个组件及通知 editor 均被移除。Notification 检查真实 Tab 后的操作按钮四边 focus、fill 及通知背景；Timeline 在正反序中分别检查原生保存按钮四边 focus。Message 是非焦点信息，没有将其描述为需要键盘焦点环。原 Popover、retained editor、真实反馈、preedit 和裁剪断言保留。

身份范围按实际断言记录：ComponentId 和 editor 含完整 generation；Timeline 外部 ref 实际执行恢复聚焦。Popover/Notification 的公开 ref 方法未被测试调用，不扩大为所有 slot/ref 代际或方法均经实测。

## 旧流程代表

原 TimePicker 定向回归首次失败，原始输出保留于 `out/091-old-time-picker.log`：其目录预期列表仍把新增四个入口当成参考卡片。TimePicker 和 DatePicker 测试各增加 Popover、Message/Notification、Timeline 的精确实际 heading 预期，其余 fallback 与原精确 offset 断言不变。

使用本次正式 MSVC Debug 新构建产物直接运行：

```text
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --details-loading-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --time-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
```

- Descriptions/Skeleton：exit 0，227.0746923 秒，`out/091-old-details-loading.log`。运行在 anchor 夹具补齐前、实施阶段的新构建程序；它所用生产源码与该流程断言未被后续修复改变，不冒充修复后重新执行。
- TimePicker：补齐后 exit 0，250.5290788 秒，`out/091-old-time-picker-fixed.log`。
- DatePicker：补齐后 exit 0，216.1123332 秒，`out/091-old-date-picker-fixed.log`。

后两项使用与 `r1-both-fixed` 相同的最终程序，保留真实目录、草稿/preedit、受控回写及无失焦同值 Form reset 断言。没有重复完整 Gallery 或全部历史 scoped flags。

## 最终定向与保留性

`r1-both-fixed` 新流程完整业务路径、三主题双宽 6/6、最终 inventory、精确八项目录 contracts 8/8 均通过，contracts real 0.27 秒。inventory 与实施阶段相同，仍为 390 IDs、389 Buttons、1439 interactions、693 visible、295 live、153 Inputs。`compare-baseline.py` 再次确认 358 旧 IDs 原序、33 旧样例函数体/调用顺序、69 非目标条目和 Core/API/Theme/goldens/CMake 保留。clang-format 22.1.3 检查 723 files、0 failures。

最终三个 exe 的 SHA-256：

| 相对 `out/build/windows-msvc/` 路径 | SHA-256 |
| --- | --- |
| `tests/Debug/rynui_token_gallery_frame_tests.exe` | `B07FBAC8AACA12386B3940A94D8A21A1F82DFBCF319B2C9E73C308BA72586CFB` |
| `examples/Debug/rynui_token_gallery.exe` | `BAECF79FE1931F9E2AED13767FF1D358A668063A10654CAD3C00739CFAE64DE7` |
| `tests/Debug/rynui_ant_design_reference_catalog_tests.exe` | `3499C5208C75BC478D7A9B607AC1C41F63862E3276CA95D7934DDDCC00C1B0FF` |

提供新 R2 审查的两个源码 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `examples/token_gallery/token_gallery_definition.cpp` | `B7A60EE09EB5A2708340C6B00E2EE5395A412093F216C4F45E078F75C954C6E6` |
| `tests/token_gallery_frame_tests.cpp` | `30718E58991EE06133F46963059DF1E5788E15A4B11EABD4227BA0246DE9B72B` |

## 独立 R2 结论

Astra xhigh / default 的全新 R2 完整审查初始 11 文件差异和上述两个源码文件的修复，未发现新的可达 P1/P2，R1 两项均关闭。独立直接执行现有 frame exe 一次 `--overlays-timeline-only`，完整业务路径及六组矩阵全部 exit 0；短 catalog、baseline、generator check 和 diff check 通过。没有重复 inventory、八项 contracts 或父方旧代表。

本轮对象明确为 ROOT `2a5989fa` 加预期 dirty 修复，不是该 HEAD 单独内容。两份源码 SHA-256 与本表一致，审查前后未变；Git blob 分别为 `444c8246c664aa8f3dd12ff0c6b2a4e8df518a22` 和 `fafb04c9679597b50ba4adf0c28cb2a38c469867`，提交时按同一 blob 关联审查结论。三个 exe 哈希前后均保持。

父方已实际重新构建并执行本节列明的检查；独立审查执行的是已有二进制，不能描述为它重新构建。显式 OpenSpec 1.14.0 doctor healthy、strict 88/88 与最终 diff check 通过。任务 3.1 据上述组合证据收口，完整 Gallery/native/GPU、人工视觉、Linux、系统 IME、AT 均保持本 change 原有边界。
