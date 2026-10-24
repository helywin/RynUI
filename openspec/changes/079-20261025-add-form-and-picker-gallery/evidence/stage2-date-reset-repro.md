# DatePicker String Form 同值 reset 复现

2026-10-25，独立 `t3/form-gallery-workflow` 工作树，固定基线 `d89eac780eb4eb981813a5704dd51e7e79d4b2a9` + 已验证 fixture 修复 `b28bd9523c6efd3782e363562911a0e883448158`。未导入 TimePicker 或父工作树 WIP；未修改 Core/Theme。

新的 Gallery 日期 Form 使用空初值的 `FormField<String>`，FormItem 连接真实 `DatePickerRef`（InputRef alias）；DatePicker.value 为 `bind([field] { return parse_iso_date(field.get().bytes()); })`，onChange 在 FormItem content 内捕获当前注册代际并用 `format_iso_date` / 空字符串回写。

实际操作和结果：

1. 通过真实日期 Input 的 Ctrl+A + TextCommitted 输入 `2026-10-`；不按 Enter，字段 authority 仍为空。
2. 保持 Input 焦点，调用同一已挂载原生重置 Button 的 activation handler，其业务回调只调用 `FormRef.reset()`，避免指针失焦先格式化草稿。
3. Form reset 后 picker snapshot.draft 和真实 TextEditor.value 均仍为 `2026-10-`，不是空初值。

在官方 Windows MSVC `windows-msvc-debug` / Ninja Multi-Config、本树独立构建、最多 4 jobs 下执行：

```text
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_document_model_tests rynui_gallery_document_viewport_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
```

build 退出 0，补强后的聚焦测试退出 **1**：

```text
DatePicker same-value String Form reset retained draft before blur: picker=2026-10- editor=2026-10-
```

最终将回归放到独立 clip/身份矩阵之后运行，原始 stdout/stderr 保存于本树 `out/date-gallery-same-value-reset-failure.log`，记录命令为 `cmd.exe /d /c "out\build\windows-msvc\tests\Debug\rynui_token_gallery_frame_tests.exe --date-picker-only > out\date-gallery-same-value-reset-failure.log 2>&1"`。前两行分别确认真实目录/日期流程和 Default/Dark/Compact × 1200/560×900 的 glyph/surface/hit clip、编辑 gate、保留身份通过，第三行为上述精确失败；总退出码仍为 1。

`src/component/date_picker_component.cpp` 的 Input.value 绑定 `s.display`（450 行），真实输入回写该 draft（459 行）；受控值订阅只在 `s->model.value() != value` 时清草稿（549 行）。`src/component/input_component.cpp:2896` 的通用 Form reset 从该 Input 的 authoritative_value 恢复（2920 行），读到的仍为 DatePicker 草稿。同值 String reset 不触发 DatePicker authority 变化，所以问题不会由普通 Input ref 的 reset 消除。

修复导入前的首轮日期聚焦测试已通过真实目录、键盘/指针月历、ISO 输入、min/max/day13 拒绝、清空、String Form 提交和普通原生按钮 reset，以及 Default/Dark/Compact × 1200/560×900 的 glyph/surface/hit clip/身份/编辑 gate；追加日期后 panels/forms 也分别退出 0。该首轮普通 reset 包含真实失焦，因此不能作为本问题通过的证据。当时 task 2.1 保持未完成，先向父线程报告实际复现，由父线程处理组件层修复。

## 独立修复整合与最终复验

按父线程明确授权，仅执行 `git merge --ff-only 0e0f11f7cb903f6b629ce02d5561621fbdb5cee4`，保留并恢复重叠的 Gallery 文档改动；未导入 TimePicker，没有自行修改 Core。原失败断言保留，又增加 CompositionChanged preedit 与焦点/开合/身份检查。

最终目录版本在本树 MSVC `windows-msvc-debug` 重新构建，`--date-picker-only` 实际退出 **0**，`out/date-gallery-final-focused.log` 最后一行为：

```text
DatePicker same-value String Form reset before blur cleared draft/preedit and retained focus/open/identity
```

同批 `--forms-only`、`--panels-only`、`--inventory-only` 均退出 0，完整阶段命令、结果和剩余验收边界见 [DatePicker Gallery 阶段证据](stage2-date-gallery.md)。本复现已由独立修复解决，旧失败日志仍保留。
