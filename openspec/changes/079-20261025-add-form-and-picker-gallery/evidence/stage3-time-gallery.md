# 079 阶段 3：TimePicker Gallery 平台通用验证

## 基线与范围

2026-10-25，分支 `t3/form-gallery-workflow`，工作树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-e1caba93`。先确认 tracked/untracked 状态为空、HEAD 为 `356995e3d212ff44237bf4aa3a3d9ad0a732822c`，再按父线程授权执行 `git merge --ff-only 580a8c3d235c22eb95b11688b509ee1e2ea65e44`。该已提交基线包含验收后的 TimePicker 核心、导航组合修复、082 Gallery 与原 Date/Form reset 修复；没有从其他 dirty 工作树复制文件，也没有 rebase。

本阶段只追加 task 3.1。Core/source/header、Theme、golden、CMake/ABI、其他 OpenSpec、README/completion 均无实施改动。没有 push、PR、archive、子任务或父 checkout 编辑。最终完整 frame/原生 smoke 与新的独立 Astra 复核留给父线程 task 4.1；本阶段的 Recording 不代表原生 GPU、Linux、系统 IME、人工视觉或辅助技术验收。

## 实现与实际交互

- 在既有 Steps 导航组之后追加两个真实 TimePicker。独立样例的 value/open 受控；固定分钟精度、初值与空值面板 `08:00`，范围 `08:00`–`18:00`、15 分钟步长、12 时不可选，不读取时钟。确认/即时模式、锁定已保存值、文本只读与恢复按钮可操作，反馈来自真实 authority 和选择请求。
- 使用严格 TextCommitted 输入、时/分列实际 pointer 与键盘选择、滚轮只滚对应列、OK/Cancel/Escape、native clear。锁定值时请求被拒绝且关闭后显示回到接受值；inputReadOnly 只禁止文本，列仍可选择。保留 footer 备注 Input 与原生 Enter 行为。
- 独立的第三个 Form 使用 required `FormField<String>`，固定秒精度和初值 `08:00:00`；validator 检查非空程序值的严格格式、范围及禁用小时。Binding 调用公开 parse_time_of_day；在 FormItem content 内捕获注册代际 onChange，以 format_time_of_day 回写真实接受请求。FormValue 仍只有原四类 typed 值。
- `FormItem.control(TimePickerRef)` 连接实际 InputRef alias，验证失败聚焦真实 picker。Form disabled/readOnly 使用 field.uneditable gate 整个 picker；恢复按钮均在 Form 外。成功反馈取实际 FormResult.values；验证失败、清空、真实按钮 reset、禁用拒绝提交和只读允许提交均执行。
- 新的 `--time-picker-only` 独立入口同时纳入完整 frame；通过稳定标题/字段标签的后代归属选择独立 picker、Form、FormItem、editor 与 footer，不按全局 mounted.back/front 或单字段数量寻找 Form。
- 目录测试点击实际可见 TimePicker Button，消费生产 navigation request，使用共同 gallery_component_navigation_anchor/jump 到实际标题与 selector；逐项检查其他组件的既有锚点。后续 reveal 只用于操作控件，不充当目录跳转证明。
- Default/Dark/Compact × 1200×900 / 560×900 检查实际 ordered glyph、精确 popup 背景 quad/选中行 highlight quad、列/footer 的有效 HitTest clip 与真实 pointer 命中。检查各列滚动、footer/editor/rows/controls/scene 身份、普通帮助/错误/Theme/宽度更新中的 draft/preedit 保留，以及 disabled/readOnly 恢复。
- 每组 Theme/宽度均沿 Date 的同一 native Button activation handler 调用公开 Form reset，保持 Input 聚焦、不给 pointer blur 先修复草稿的机会。authority 同为 `08:00:00` 时，完整 `09:30:00` 和未完成 `09:` 草稿连同 Composition preedit 均清除；accepted/pending/显示恢复初值、焦点/open/身份保持，随后 Enter 和实际 Form submit 不复活旧值。

## 实际命令与结果

本树官方 `windows-msvc` / `windows-msvc-debug`，MSVC **19.51.36256.0**、Ninja Multi-Config，最多 **4 jobs**。本地 helper `out/form-gallery-build.ps1` 进入 VS x64 环境并调用下列 build；首次导入基线时自动重新 configure。Python **3.10.11**，clang-format **22.1.3**。

```text
git merge --ff-only 580a8c3d235c22eb95b11688b509ee1e2ea65e44
python tools/update_ant_design_gallery_catalog.py --write
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_document_model_tests rynui_gallery_document_viewport_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --time-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --navigation-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --panels-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --states-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

先运行真实 TimePicker 聚焦通过，才移除 TimePicker overlay 的 Gallery 缺口，保留 partial 与所有组件/平台限制，增加实际范围并生成目录。随后重新构建最终版本，加入选中行 quad clip、禁用小时键盘跳过和门禁提交断言后复验。没有发现需要向 Core 提交修复的新问题。

最终 build、TimePicker/navigation/DatePicker/Form/panels/states 聚焦均退出 **0**。states 额外复验其既有目录合同对新增 TimePicker 明确锚点的检查。原 Form 的 `1.0000004` / `1.234567891234` 精度断言、Collapse body 后代 Select 归属断言、Date y=780 操作的真实 glyph/hit 检查及其无失焦 reset 断言均保留并通过。八项 contracts **8/8**（**0.55 秒**），包含 generator check/self-test；格式检查 **646** 文件/**0 failures**，doctor **healthy**，strict **81/81**。

最终 TimePicker 原始日志 `out/time-gallery-final-focused.log`：

```text
TimePicker real catalog/columns/pointer/keyboard/wheel/strict input/authority/OK/Cancel/Escape/clear/String Form passed
TimePicker theme_step=4 viewport=1200 finite clip/draft/gates/no-blur reset passed
TimePicker theme_step=4 viewport=560 finite clip/draft/gates/no-blur reset passed
TimePicker theme_step=0 viewport=1200 finite clip/draft/gates/no-blur reset passed
TimePicker theme_step=0 viewport=560 finite clip/draft/gates/no-blur reset passed
TimePicker theme_step=1 viewport=1200 finite clip/draft/gates/no-blur reset passed
TimePicker theme_step=1 viewport=560 finite clip/draft/gates/no-blur reset passed
TimePicker Default/Dark/Compact x 1200/560 x 900 actual glyph/quad/hit clip, gates, draft/preedit and retained identity passed
TimePicker same-value String Form no-blur reset restored accepted/pending/display, cleared complete/incomplete draft/preedit, retained focus/open/identity and prevented Enter revival
```

其他原始日志为 `out/time-gallery-build-final.log`、`out/time-gallery-final-{navigation,date,forms,panels,states,inventory}.log`、`out/time-gallery-contracts.log`、`out/time-gallery-format.log`、`out/time-gallery-doctor.json`、`out/time-gallery-strict.log`。可用 Windows cmd 原样重定向 stdout/stderr：`cmd /d /c "out\build\windows-msvc\tests\Debug\rynui_token_gallery_frame_tests.exe --time-picker-only > out\time-gallery-final-focused.log 2>&1"`。

最终目录版本 `--inventory-only` 实际退出 **0**，输出如下；只有这个既有 inventory fixture 使用 60000 高视口，所有样例可达性与 clip 测试保持 900 高。

```text
stable_ids=334 navigation=93 buttons=350 interactions=1380 visible_interactions=647 live_samples=286 theme_content=67 inputs=147 selects=22 modal_drawer=13 selection=46 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=3 form_items=6 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1
```

## 保留合同与阶段边界

与指定基线 `580a8c3` 的逐项比较通过：**30** 个旧 add_* 样例函数体逐字相同；**319** 个旧稳定 IDs 是新列表的原顺序前缀，新增 **15** 个且没有重复；原样例调用顺序保留，TimePicker 只追加在最后；所有非 TimePicker support overlay 条目完全相同。Form/DatePicker/Empty/Result/Breadcrumb/Steps 及其他组件导航语义保留，只新增明确的 TimePicker → `gallery.time-picker.controlled` 映射。

本阶段不运行完整 Gallery frame/native smoke，不修改 task 4.1 或 completion 清单，不把平台通用 Recording 与组件核心的前置验收合并计数。

task 3.1 完成；079 整体 **3/4**，task 4.1 保持未完成。
