# Astra R2：Collapse 子 Select 定位回归

2026-10-25，在 `t3/form-gallery-workflow` / `C:/Users/jiang/.t3/worktrees/RynUI/t3-e1caba93` 从干净 `15ad812` 执行 `git merge --ff-only d89eac780eb4eb981813a5704dd51e7e79d4b2a9`。仅取该已提交基线；没有导入父工作树未提交的 TimePicker merge/WIP。

修复前，用本工作树重新构建的 Windows MSVC Debug binary 实际执行 `--panels-only`，退出码 **1**，输出：

```text
Collapse Gallery child Select belongs to another sample
```

原因是旧 fixture 使用全局 `services.select().mounted().back()`，追加的 Form role 成为最后一个 Select。修复只在真实 Collapse 首个 panel body 后代中查找其 Select，增加缺失检查，保留原归属断言和全部行为/clip/身份断言；不修改旧生产样例函数。

审计同文件其他尾部定位：header/site-header 是外层最后子节点，追加发生在 document 内，不受影响；Empty/Result Input 按 `gallery.result.draft` 内容和 Result content 归属定位，不依赖末尾；其他旧 component 入口使用稳定前序或自身归属。原四字段 Form 的尾部 role/Checkbox 在当前基线仍属于其字段；Date 阶段增加第二个 Form 时单独改成对应 Form/字段的归属定位，不把全局计数作为身份。

实际命令：

```text
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_document_model_tests rynui_gallery_document_viewport_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --panels-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

修复后 build、panels、forms、inventory 均退出 **0**；八项 contracts **8/8**（0.52 秒）；clang-format 22.1.3 检查 **609** 文件、0 failures；doctor healthy，strict **80/80**，diff check 通过。`--forms-only` 同时实际覆盖父基线保留的 `1.0000004` 和 `1.234567891234` 精度断言，未修改 `to_chars` 实现或这些断言。

inventory 保持：`stable_ids=285 navigation=93 buttons=314 interactions=923 visible_interactions=596 live_samples=280 theme_content=67 inputs=138 selects=22 modal_drawer=13 selection=46 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=1 form_items=4`。

本阶段只改 fixture 和此证据，079 保持 1/4。不重复完整 frame/native smoke，不宣称 GPU、人工视觉、系统 IME 或 Linux 验收。ignored 构建日志为 `out/form-gallery-r2-before-build.log`、`out/form-gallery-r2-after-build.log`；未 push 或创建 PR/子任务。
