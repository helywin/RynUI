# 079 阶段 1：Form Gallery 平台通用证据

## 范围和环境

- 日期：2026-10-25；仅完成 task 1.1。task 2.1、3.1、4.1 保持待办。
- 分支：`t3/form-gallery-workflow`（初始名 `t3/e1caba93`）；独立工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-e1caba93`。
- 基线：`6f3e794092d183f7b53c21bbf3a481f095431b5d`。075 已存在；没有集成 077/078，没有增加日期/时间占位或 Form API。
- Windows x64，MSVC 19.51.36256，官方 `windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config，最多 4 jobs。
- 构建目录为本工作树自己的 `out/build/windows-msvc`。只复用已有依赖源码和锁定 DXC host 输入；SDL、shadercross、库、对象、Gallery 和测试程序均在本树独立构建。
- Python 3.10.11，clang-format 22.1.3。Recording 使用锁定验证字体；不声明原生 GPU、人工视觉、系统字体、系统 IME、Linux 或辅助技术验收。

## 实际实现与回归

样例追加在全部 Empty/Result 之后，保存四个 FormField、FormRef 和真实控件 ref。name/String、role/SelectSelection、count/InputNumberValue、consent/bool 均 required，count 另有至少为 1 的同步 validator；count 初值为 1，其余为空/false。Input、Select、InputNumber 显式连接 value/onChange/status/edit gate，Checkbox 按已有 API 连接 checked/onChange/uneditable，反馈由 FormItem 保留 Text 显示。每个 FormItem 的 control 都连接实际 ref。

六个原生按钮执行保存、校验、重置、禁用、只读和帮助更新；位于 Form 外，禁用后仍能恢复。成功反馈遍历实际 FormResult.values，未直接读取字段伪造结果。

`--forms-only` 已纳入完整 main 入口，最终退出码 0，覆盖：

- 实际可见 Form 目录 Button → 生产 navigation request → 共用 `gallery_component_navigation_anchor` → `jump_to_component` → 真实标题和可聚焦 Input。Default/Dark/Compact × 1200×900 / 560×900 全部执行；没有通过手工 sampleY 代替目录入口。
- 真正 TextCommitted / Ctrl+A / Backspace，Select 弹层真实行指针选择及 Checkbox Space 勾选，验证四类值进入真实成功反馈。
- 初次失败聚焦 name，随后依次验证 Select ref、空数字 required、数字 0 的 custom validator 和 Checkbox ref 的失败焦点；实际 retained error Text/glyph 和 Input status。
- 原生按钮 reset 清除错误，恢复空名称/空角色/数量 1/false，并检查实际数字显示。
- 真实输入 `1.`，authority 仍为 1；同一原生 Button 命令的补充无失焦调用验证普通 validate/error/help/Theme/宽度更新保留数字草稿与 CompositionChanged preedit，reset 在任何失焦前将实际显示恢复为 `1` 并取消 preedit。该补充检查不冒充系统 IME/原生指针 reset；真实按钮 reset 另有独立覆盖。
- 六组有限视口中的实际 label、help/error、数字与状态 glyph，场景 ordered commands、有限位置/translation/clip、包含它们的实际 item/document 分配区域和视口边界，以及可见目标的真实 HitTest/焦点。Text ink 可超出 advance；新增样例设置 1dp inset，未扩大 glyph 断言容差，也未把 required marker 的 advance box 当作字形裁剪区。
- 六组主题/宽度中的真实禁用/只读 props、Select/Checkbox 拒绝操作、只读 Input 拒绝文本、disabled submit 拒绝、readOnly submit 允许校验且不将焦点移给不可编辑字段，以及外部按钮恢复编辑。
- 所有旧/新 Input 的 component/editor/interaction 顺序，全部 mounted Text component/scene 和 Button component/interaction/scene 顺序，FormItem 三个 slot、Select/Checkbox 身份、总 component/mount 次数及顶层/Theme content 次数均保留。

源码差异核对同时确认：26 个旧 `add_*` 样例函数体保持原样，273 个原稳定 ID 原序保留，仅末尾追加 12 个 Form ID；support overlay 除 Form 外所有条目完全相同。生产导航只新增 Form 映射，保留 Empty/Result 映射，并在聚焦测试中核对其余 identity 仍指向原 reference card。

最终聚焦输出：

```text
Form real navigation, required/custom focus, typed events, success values and pointer reset passed
Form same-value numeric draft and preedit matrix/reset passed
Form Gallery: real directory/input/select/checkbox/submit/reset/gates, same-value 1. draft, preedit, retained identity and Default/Dark/Compact x 1200/560 x 900 passed
```

## 实际命令

忽略的 `out/form-gallery-build.ps1` 进入本机 VS x64 Developer Environment 后执行：

```text
cmake --preset windows-msvc
  -DPython3_EXECUTABLE=C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_freetype-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_harfbuzz-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_utf8proc-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_sheenbidi-src
  -DFETCHCONTENT_SOURCE_DIR_SDL3=D:/code/RynUI/out/build/windows-msvc/_deps/sdl3-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SDL_SHADERCROSS=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_sdl_shadercross-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SPIRV_CROSS=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_spirv_cross-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_DXC=D:/code/RynUI/out/build/windows-msvc/_deps/rynui_dxc-src
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_document_model_tests rynui_gallery_document_viewport_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --forms-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
python tools/update_ant_design_gallery_catalog.py --write
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

configure、全部指定 targets 和最终聚焦回归退出码均为 0。八项 contracts **8/8**，总时间 0.53 秒；generator check/self-test 包含在内。格式检查 **596** 个自有源文件，**0 failures**；doctor **healthy**，strict **79/79**；diff check 通过。构建中保留既有第三方/未使用参数等 warnings，未将其当作验收证据。

本机 ignored 辅助文件为 `out/form-gallery-build.ps1`、`out/form-gallery-build-final.log` 和 `out/verify-form-append.py`，不进入提交。空文本的删除回归最终使用真实 Backspace；TextCommitted 空事件不会代替用户删除动作。未发现需要修改 Core 的问题。

## 实测 inventory

最终 `--inventory-only` 输出（60000 高仅用于原有全量 inventory fixture，不作为可达性证据）：

```text
stable_ids=285 navigation=93 buttons=314 interactions=923 visible_interactions=596 live_samples=280 theme_content=67 inputs=138 selects=22 modal_drawer=13 selection=46 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=1 form_items=4
```

完整 frame 中的 inventory 断言按上述实际数值更新，没有从控件数量猜测；既有样例位置/ID 顺序保留。

## 剩余边界

Form 支持目录仍为 partial，只移除 interactive Gallery 缺口并增加本阶段真实范围，保留全部平台和首版 API 限制。未修改 Core、Theme、其他组件/支持条目、README 或其他 OpenSpec。

本阶段没有重复完整 token_gallery_frame、原生 GPU/`--smoke` 或父分支 headless 矩阵。DatePicker、TimePicker 和全部样例齐备后的完整 Gallery/Astra 集成验收分别由 2.1、3.1、4.1 记录，均保持未完成。本次仅创建本地阶段提交；未 push、创建 PR、archive、子任务或操作父工作树。
