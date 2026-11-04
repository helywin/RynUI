# TimePicker Linux 平台通用验收

- 上游固定 Ant Design **6.6.5** / `4a39f54842eade4e565ab336ef6097cd7e723cdd`；源码/types/demos 副本仅在忽略的 `out/date-time/upstream/`。
- 实际环境：Linux x64、GCC **16.2.1**、CMake **4.4.4**、`linux-gcc-headless` / Ninja Multi-Config，Debug/Release，始终 `-j2`。BUNDLED 使用原锁定源码缓存，无新增依赖。
- 实现：旧 TimeOfDay/单值 API 兼容；整数毫秒、真实 12/24 小时与 AM/PM、locale 格式、所有步长边界、依赖禁用规则；typed TimeRangePicker 可空端点、排序与 pending/accepted authority；显式 Now/惰性预设、needConfirm/changeOnScroll、保留富 slots、RTL/定位/有限动画。覆盖午夜/正午、非法部分草稿、精确事件次序、同值 Form reset、嵌套 popup/preedit、异常/自销毁和 wheel 实际边界/有限空闲。
- 实际独立 Gallery TimePicker 页挂载 **5** 个 TimePicker；12/24 小时、毫秒/Now/预设、scroll-select 与范围 authority 控件可交互，520 宽更新保留身份。支持 overlay 在实现和自动检查完成后才标为 implemented。
- 精确构建：分别用 `debug`、`release` 替换 `<config>`：

```sh
cmake --build --preset linux-gcc-headless-<config> --target \
  rynui_picker_completion_tests rynui_picker_gallery_tests \
  rynui_time_picker_model_tests rynui_time_picker_component_tests rynui_time_picker_review_tests \
  rynui_time_picker_public_api_tests rynui_time_picker_theme_tests rynui_time_picker_integration_tests \
  rynui_date_picker_model_tests rynui_date_picker_component_tests rynui_date_picker_public_api_tests \
  rynui_date_picker_theme_tests rynui_date_picker_review_tests rynui_gallery_sample_page_tests \
  rynui_gallery_page_model_tests rynui_token_gallery_frame_tests rynui_portable_theme_public_api \
  rynui_ant_design_reference_catalog_tests -j2
ctest --preset linux-gcc-headless-<config> -R '^rynui\.(picker_completion_|(date|time)_picker($|_)|gallery_page_(date-picker|time-picker)_frame$|gallery_(sample_page|page_model|sample_source_contract|sample_source_generated)$|portable\.(theme_public_api|backend_boundaries)$|ant_design_(reference_catalog|gallery_catalog_generator|gallery_catalog_contract)$)' --output-on-failure -j2
```

- 结果：相关 **44/44 Debug、44/44 Release**，含 11 组新增 picker 合同、2 个真实 Gallery 页面交互合同、2 个 Recording scene 页面 frame，以及旧 picker/review、公开头、Theme 和 Core/backend 边界。未运行全量 CTest。
- 最终 catalog 修改后另建 `rynui_ant_design_reference_catalog_tests rynui_picker_gallery_tests rynui_gallery_page_model_tests rynui_gallery_sample_page_tests`，同 preset / `-j2`；以 `-R '^rynui\.(ant_design_(reference_catalog|gallery_catalog_generator|gallery_catalog_contract)|gallery_(page_model|sample_page|sample_source_contract|sample_source_generated)|(date|time)_picker_completion_gallery)$'` 运行，**9/9 Debug、9/9 Release**。
- `python tools/update_ant_design_gallery_catalog.py --write --check --self-test`、`python tools/generate_gallery_source.py --output out/date-time/generated_gallery_source.inc` 及同命令 `--check`、`python tests/gallery_source_tests.py` 通过：本隔离基线 **84** 个实际源码样例（79+5），**458** 个 frozen legacy IDs 保留。父级较新样例由集成方重算，不在此分支冒充已验证。
- `python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：clang-format **22.1.3**，**892** 个自有源码，0 failures。两个 docs 的全部 C++ 示例提取至忽略 out 后以 `c++ -std=c++20 -Iinclude -fsyntax-only out/date-time/docs-examples.cpp` 通过。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` **107/107**；`git diff --check` 通过。原始日志、JSON、生成源码和二进制只在忽略的 out。
- 边界：HEADLESS/Core 合成输入与 Recording scene 通过；未运行原生 GPU/Wayland 窗口、实际 Windows MSVC、物理系统 IME 候选 UI、辅助技术或跨显示器验收。这些是独立平台边界，不建立人工 checkbox；63.2、64.2、65.1 保持父级待完成。无公开 Calendar、Form core、renderer/SDL/GPU ABI 或 Theme 字段改动；只更新本组件 overlay。仅本地提交，无 push/PR/archive。
