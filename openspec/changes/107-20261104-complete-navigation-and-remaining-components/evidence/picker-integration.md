# DatePicker / TimePicker main 集成结果

在 `b9378afe` 的 Cascader/partial Transfer 基线上集成 `t3/complete-date-time-pickers` 的 `be53825d`、`f751d1ec`、`c4044eee`。独立分支的 44/44 与收尾 9/9 仍见 `date-picker-linux.md` / `time-picker-linux.md`，不冒充当前 main 的运行结果。

- 当前 Linux GCC 16.2.1 / Ninja Multi-Config，`linux-gcc-headless` Debug **33/33**、Release **33/33**：11 组 picker completion、公开 API/Theme、同值 Form String reset、Time/Navigation 共存、Form 组件、两个 Recording 页面 frame、Gallery/源码/目录与 Core/backend 边界。两份文档的全部 C++ 示例以 `c++ -std=c++20 -Iinclude -fsyntax-only out/picker-integration/docs-examples.cpp` 编译通过。未运行全量 CTest、旧 Select 或导航 GPU 矩阵。
- `linux-gcc` Release 原生 Gallery 经 `run_gallery_page_acceptance.py` → `run_background_wayland.py`，**DatePicker → TimePicker → Button：3 cases / 647 checks / 134 submitted frames，0 失败**；实际 Vulkan/SPIR-V、系统字体、host input area/cursor 同步通过，无桌面 fallback。
- 日期实际验证五种面板、locale 周起点/RTL、保留 footer 草稿、Today/预设、多选 pending/accepted 拒绝及接受、日期与日期时间范围/毫秒预设、Form String reset、子浮层 preedit/Escape 与离页销毁。时间实际验证 AM/PM 午夜映射、毫秒键盘步进/确认、Now/预设、changeOnScroll pending/确认、12/24 小时、范围拒绝/接受、保留草稿、Form reset 与 preedit 离页清理。
- 初次原生驱动误要求只读的多选 selector 建立可编辑 session；第二次给默认小写 `h:mm a` 输入了大写 AM/PM，被解析器正确拒绝。分别改为普通键盘聚焦/真实单元格指针点击和公开 formatter 生成输入后通过；生产组件未因此放宽合同。
- 实际源码样例 **87**（集成前 82，picker 新增 5）；冻结 **458** 个 legacy ID 及归属文件保持不变。完整 Theme golden/字段、Input/InputNumber、Form core、renderer/SDL/GPU ABI、依赖锁与 BUNDLED 模式均未改动。Transfer 继续保留 partial 状态。

- 更新实际 Linux 原生支持说明后，目录/源码/Gallery 子集 Debug **9/9**、Release **9/9**；重建最终 Release Gallery 并再次通过上述 **3/647/134** 原生序列。clang-format **22.1.3** 检查 **922** 个自有源码，0 失败；源码生成 `--check`、catalog `--check --self-test`、doctor healthy、strict **107/107** 与 diff 检查通过。
- 仅关闭 12.1/12.2/22.1/22.2，保留 Cascader 与原有完成项，进度 **29/135**。

实际共同 targets 与命令：

```sh
suite='^rynui\.(picker_completion_|(date|time)_picker_(completion_gallery|public_api|theme)$|date_picker_review_form_reset$|time_picker_review_(reset|cycles)$|time_navigation_merge$|form_component$|gallery_page_(date-picker|time-picker)_frame$|gallery_(sample_page|source_panel|page_model|sample_source_contract|sample_source_generated)$|ant_design_(reference_catalog|gallery_catalog_generator|gallery_catalog_contract)$|portable.backend_boundaries$)'
targets=(rynui_picker_completion_tests rynui_picker_gallery_tests rynui_date_picker_review_tests rynui_time_picker_review_tests rynui_date_picker_theme_tests rynui_time_picker_theme_tests rynui_date_picker_public_api_tests rynui_time_picker_public_api_tests rynui_time_navigation_merge_tests rynui_form_component_tests rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests rynui_ant_design_reference_catalog_tests rynui_gallery_page_model_tests rynui_token_gallery_frame_tests)
cmake --preset linux-gcc-headless
# 对 debug、release 分别执行：
cmake --build --preset linux-gcc-headless-debug --target "${targets[@]}" -j2
ctest --preset linux-gcc-headless-debug -R "$suite" --output-on-failure -j2
cmake --build --preset linux-gcc-headless-release --target "${targets[@]}" -j2
ctest --preset linux-gcc-headless-release -R "$suite" --output-on-failure -j2
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page date-picker --page time-picker --page button --timeout 120 --log-dir out/picker-integration/native-catalog-final
python tools/update_ant_design_gallery_catalog.py --write --check --self-test
python tools/generate_gallery_source.py --output out/build/linux-gcc-headless/gallery-generated/generated_gallery_source.inc --check
python tests/gallery_source_tests.py
python scripts/format-code.py --check --clang-format out/linux-validation/format-tools/bin/clang-format
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

Windows/MSVC、物理系统 IME 候选 UI、辅助技术、真实桌面及跨显示器未验证；Core composition 注入及后台 GPU 不替代这些证据。整个数据录入 63.2、Windows 64.2、最终 65.1 和 Transfer 23.* 保持未完成。原始日志、JSON、源码生成物、图片及二进制仅在忽略的 out。
