# 079 阶段 2：DatePicker Gallery 平台通用验证

## 基线与范围

2026-10-25，分支 `t3/form-gallery-workflow`，工作树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-e1caba93`。从干净 `15ad812` 仅快进到父线程指定的已提交基线 `d89eac780eb4eb981813a5704dd51e7e79d4b2a9`，再完成独立 fixture 修复提交 `b28bd9523c6efd3782e363562911a0e883448158`。未导入任何 TimePicker merge/WIP，未修改 Core/Theme、其他组件/OpenSpec、README 或父工作树。

发现 DatePicker 同值 String Form reset 保留草稿后，先向父线程给出实际复现，再按明确授权仅快进到独立组件修复 `0e0f11f7cb903f6b629ce02d5561621fbdb5cee4`。该提交直接后继 `b28bd95`，不含 TimePicker。重叠的 DatePicker 文档先保存为 `out/date-gallery-doc-before-core.patch`（SHA256 `B2D7F83A80ABBAEC9EBDF62AE901B90A1C133EE6EC756F0264E8BF5C5131B9D7`），快进后通过 git apply --check 并完整恢复；其余 Gallery 改动原地保留，未 rebase。

本树最终复验全部通过，task 2.1 完成，079 整体进度为 2/4。历史失败及修复前后结果见 [reset 最小复现](stage2-date-reset-repro.md)。task 3.1/4.1 未实施。本阶段不自行修改导入修复的 Core/077 文件。

## 实现与回归

- 在原 Form 后追加受控 DatePicker，固定 value `2026-10-12`、defaultPanelDate `2026-10-01`、min/max `2026-10-05` / `2026-11-25`，每月 13 日不可选。使用公开 CivilDate/parse_iso_date/format_iso_date，不读取系统当前日期，不使用私有日历辅助函数。
- 真实月历/月份导航、键盘选择、ISO 键入、清空和保留备注 Input footer；选择值与开合均真实受控。
- 单独的 required `FormField<String>` 日期 Form，通过 Binding 解析 authority，并在 FormItem content 内捕获当前注册代际的 onChange 后格式化回写。control 连接 DatePickerRef/InputRef alias；validator 处理非空非法 ISO、范围和禁用日。成功反馈读取实际 FormResult.values，不扩展 Form typed 值。
- disabled/readOnly 通过 field.uneditable 禁用整个 picker；恢复按钮位于 Form 外。未将 inputReadOnly 误当整个选择器只读。
- 生产共用 anchor 只新增 DatePicker → 实际稳定样例标题；聚焦测试点击真实可见目录 Button，消费生产 request、共用 anchor/jump 后操作真实 Input/日历。逐个核对既有 Empty/Result/Form 标题锚点和其余原 reference-card 目标。
- `--date-picker-only` 接入完整 main；覆盖真实事件、成功/错误/焦点/清空/reset、三个 Theme × 1200×900 / 560×900 的实际 ordered glyph、popup surfaces 和 HitTest clip、footer/输入草稿、preedit、控件/slot/editor/scene 身份。保留原无失焦 reset 失败断言，不 catch/skip，并进一步检查 preedit 清除、空 authority、焦点/开合/身份保留。
- 原四字段 Form 测试改为按自己的名称标签、Form/字段后代归属寻找对应 FormItem、Select、Checkbox，并保持可见上下文；保留原断言及父基线 `1.0000004` / `1.234567891234` 精度回归。

## 已实际运行

环境与 stage 1 一致：本树独立 Windows MSVC 19.51.36256、官方 `windows-msvc` / `windows-msvc-debug`、Ninja Multi-Config，最多 4 jobs；Python 3.10.11，clang-format 22.1.3。

```text
git merge --ff-only 0e0f11f7cb903f6b629ce02d5561621fbdb5cee4
python tools/update_ant_design_gallery_catalog.py --write
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_document_model_tests rynui_gallery_document_viewport_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --panels-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

导入独立修复后，先运行含无失焦同值 reset/preedit 回归的日期聚焦，实际退出 0。随后才移除 DatePicker support overlay 的 Gallery 缺口、补充实际范围并生成目录，重新构建最终版本。

最终目录版本的 build、`--date-picker-only`、`--forms-only`、`--panels-only`、`--inventory-only` 全部退出 **0**。原 Form 真实执行 `1.0000004` 和 `1.234567891234` 精度回归，Collapse body 后代 Select 定位及原归属断言继续通过。八项 contracts **8/8**（0.71 秒），包含 generator check/self-test；格式 **609** 文件/**0 failures**，doctor **healthy**、strict **80/80**、diff check 通过。

最终日期聚焦原始输出：

```text
DatePicker real catalog/calendar/ISO/restrictions/clear/String Form submit/reset passed
DatePicker Default/Dark/Compact x 1200/560 x 900 actual glyph/surface/hit clip, gates and retained identity passed
DatePicker same-value String Form reset before blur cleared draft/preedit and retained focus/open/identity
```

ignored 日志为 `out/date-gallery-catalog-final-build.log`、`out/date-gallery-final-focused.log`、`out/date-gallery-final-forms.log`、`out/date-gallery-final-panels.log`、`out/date-gallery-final-inventory.log`。聚焦日志采用原始 stdout/stderr 重定向，例如：

```text
cmd.exe /d /c "out\build\windows-msvc\tests\Debug\rynui_token_gallery_frame_tests.exe --date-picker-only > out\date-gallery-final-focused.log 2>&1"
```

历史失败日志 `out/date-gallery-same-value-reset-failure.log` 保留，未用新结果覆盖。没有增加 Core workaround；实际通过的是导入修复后的原回归与补强断言。

源码比较已确认 **27** 个旧 add_* 样例函数体完全保留（含原四字段 Form 与 to_chars）、**285** 个旧稳定 ID 原序保留，仅末尾追加 **12** 个日期 ID；support overlay 除 DatePicker 外所有条目完全相同。DatePicker 保持 partial，原组件元数据及平台/首版 API 限制保留，只移除已验证的 Gallery 缺口并追加对应范围/证据。

## 实测 inventory

```text
stable_ids=297 navigation=93 buttons=320 interactions=1044 visible_interactions=606 live_samples=282 theme_content=67 inputs=141 selects=22 modal_drawer=13 selection=46 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=2 form_items=5 date_pickers=2
```

Frame 的 inventory 断言按实际输出更新；60000 高只用于旧全量 inventory fixture，不作为新样例可达性证据。没有运行完整 frame、原生 smoke 或重复父线程 headless 矩阵；没有 GPU、人工视觉、系统字体/IME、Linux 或辅助技术验收声明。

仅创建本地英文阶段提交，未 push、PR、archive、创建子任务或操作父工作树。剩余 TimePicker Gallery 与全组完整 frame/native smoke/Astra 集成验收由 3.1、4.1 独立收口。
