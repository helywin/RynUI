# Cascader main 集成结果

- 在 Linux GCC 16.2.1 / Ninja Multi-Config，将 `t3/complete-cascader-change107` 的 `29faf58d..dc634a6c` 八个提交作为一个验证批次合入 `6212a9c5`。只关闭 9.1/9.2，保留原有 23 项完成状态，阶段进度 **25/135**。Transfer 是同时导入的 **partial 基线**，23.* 保持未完成，源码与 `transfer-linux.md` 未宣称完整原生验收。
- `linux-gcc-headless` Debug **31/31**、Release **31/31**：Cascader 20 组场景、独立 Theme、Input/variant/InputNumber、Select/AutoComplete、ColorPicker、Popover/Escape/子浮层、焦点、Gallery 源码/目录/页面及公开 Theme/Core 边界。新增受控子浮层拒绝、祖先关闭抑制和 sibling 隔离，以及 Cascader/ColorPicker Theme 共存、四个 Theme accessor 外线程拒绝回归。未运行全量 CTest 或旧导航 GPU 矩阵。
- 五份完整 Theme golden 从合并代码重新生成，逐字段比较 `6212a9c5`：原有 **59** 个顶层字段均保留，除 identity 外值完全相同，仅新增 cascader；保留 ColorPicker/Menu/Dropdown/Anchor/Layout/Splitter/FloatButton。现有 Input 和精确十进制 InputNumber 实现未改写。
- `linux-gcc` Release Gallery 已重建。仅通过 `run_gallery_page_acceptance.py` → `run_background_wayland.py`，执行 **Cascader → Select → Button：3 cases / 297 checks / 63 submitted frames，0 失败**。真实 Vulkan/SPIR-V、系统字体、host input area/cursor 同步通过；实际执行可见 tag 删除、responsive 可操作溢出、子浮层 preedit/Escape 最内层优先、本地 loader 错误/Retry、Unicode 查询、小数滚轮、End 焦点显露、preedit 离页清理及新 Select host 交互。无桌面 fallback。
- 首轮 Debug **30/31**：为演示溢出将样例收窄至 280，合法的 responsive 结果隐藏全部 tag，与样例要求冲突；恢复原有 400 后上述 **31/31** 通过。初次构建 target 误用了未注册的 portable Popover 名称，修正为现有共享 target；后续构建无失败。
- 实际源码生成 **82** 组样例；冻结 **458** 个 legacy ID 及归属文件与 main 基线逐字一致。catalog `--check --self-test`、源码生成 `--check`、clang-format **22.1.3**（**916** 个自有源文件，0 失败）、doctor healthy、strict **107/107** 与 diff 检查通过。BUNDLED 模式及依赖锁未变。

实际命令按共同 targets/suite 合并列示（Debug/Release 分别执行）：

```sh
targets=(
  rynui_portable_cascader_component rynui_cascader_theme_tests rynui_theme_algorithm_tests
  rynui_theme_runtime_tests rynui_portable_input_component rynui_portable_input_variant
  rynui_portable_button_component rynui_portable_focus_lifecycle rynui_portable_theme_scope_component
  rynui_popover_component_tests rynui_popover_integration_tests rynui_popover_review_tests
  rynui_portable_transfer_component rynui_input_number_component_tests rynui_portable_select_component
  rynui_select_complete_tests rynui_auto_complete_complete_tests rynui_color_picker_component_tests
  rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests rynui_gallery_source_panel_tests
  rynui_portable_theme_public_api
)
suite='^(rynui\.(cascader_theme|theme_algorithm|theme_runtime|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog|color_picker_component|input_number_component|select_complete_(window|retained|lifetime)|auto_complete_complete_(window|form_editor)|popover_(retained|boundaries|lifecycle|gates|children|covered)|popover_review_(escape|focus))|rynui.portable.(cascader_component|transfer_component|input_component|input_variant|button_component|focus_lifecycle|theme_scope_component|backend_boundaries|theme_public_api|select_component))$'
cmake --preset linux-gcc-headless
for config in debug release; do
  cmake --build --preset "linux-gcc-headless-$config" --target "${targets[@]}" -j2
  ctest --preset "linux-gcc-headless-$config" -R "$suite" --output-on-failure
done
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page cascader --page select --page button --timeout 120 --log-dir out/cascader-integration/native-final
python tools/generate_gallery_source.py --output out/build/linux-gcc-headless/gallery-generated/generated_gallery_source.inc --check
python tools/update_ant_design_gallery_catalog.py --check --self-test
python scripts/format-code.py --check --clang-format out/linux-validation/format-tools/bin/clang-format
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

未验证 Windows/MSVC、系统 IME 候选 UI、辅助技术、实际桌面及跨显示器体验。Core composition 注入和后台 GPU 不替代这些边界；Transfer 完整合同、整个数据录入 63.2、Windows 64.2 和最终 65.1 仍未完成。原始日志、JSON、生成源码、图片和二进制仅保存在忽略的 out。
