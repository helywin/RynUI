# ColorPicker / InputNumber 主分支组合验收

在已推送的 `13b125ae` 上，按顺序整合本地 `8451e42c`（InputNumber）、`1c39a909`（ColorPicker）、`c72d32b8`（formatter draft context）、`c2c1b067`（Gallery/docs）。来源 worktree 保持干净，没有改写或推送 T3 分支；未整合其他未完成分支。

- 实际环境：Linux x64 / GCC 16.2.1、Ninja Multi-Config。`linux-gcc-headless` Debug/Release 聚焦 **各 21/21 CTest**，0 失败；Gallery 新增实际 Token 值断言后，Debug `gallery_color_number` 再跑 **1/1**。覆盖两组件、Form、公开 Theme identity、普通 Input、Select/AutoComplete 互操作、Gallery、目录/source 和 portable backend boundary；没有重复全量 CTest 或 43 项 Select/AutoComplete 集合。
- `linux-gcc` Release 私有后台 Wayland 原生 CTest **1/1**，ColorPicker → InputNumber → Button **3/3 cases、345 checks、86 submitted frames**；Vulkan/SPIR-V/system fonts，host input-area/cursor 均为 true。保留 106 compositor frame callback、有限 pacing 和正常事件泵，无桌面 fallback。
- ColorPicker：实际 SV/hue/alpha 指针捕获与回调、HEX/RGB/HSB 格式回调、预设/clear、渐变 stop 拖动及 add/remove、typed panel/text/extra renderer、普通色值更新保留备注草稿，以及持合成 preedit/capture 的离页清理通过。
- InputNumber：`9007199254740993.00000000000000001` 通过键盘与 wheel 精确到 `.00000000000000002`、`.00000000000000003` 并交付 String 回调；逗号 formatter/parser、单调时钟长按连发、释放停止、wheel 输入优先、blur 清理 preedit、持 hold capture 的离页释放通过。Decimal 明确限制 16384 位，不经 double 往返精确值；旧 double API 与 Form/Select/AutoComplete 组合继续通过。
- 共享集成保留 InputNumber 在 Input 中的实际实现、AutoComplete friend 与既有导航。完整五份 Theme golden 各从 58 增为 59 个顶层字段；只新增 `colorPicker`，旧字段除整体 identity 外逐字段相同，包含 Menu palettes/collapsed、Dropdown arrow 和 Anchor。公开 Theme 守卫验证颜色/尺寸均影响 snapshot identity，且不污染 Select/InputNumber。
- 补齐 ColorPicker Gallery 的实际九个 Token 值显示与断言，API 示例使用实际 `Signal`。80 个 source samples、458 个冻结 legacy IDs 保持有效；没有新 shader、renderer ABI、组件私有 SDL/GPU 上传或第三方依赖。

构建均 `-j2`。HEADLESS 相关 target：`rynui_input_number_{model,public_api,component,theme}_tests rynui_color_picker_component_tests rynui_form_component_tests rynui_gallery_pages_host_tests rynui_theme_algorithm_tests rynui_portable_theme_public_api rynui_portable_input_component rynui_select_complete_tests rynui_select_component_tests rynui_auto_complete_complete_tests rynui_auto_complete_component_tests rynui_gallery_sample_page_tests rynui_ant_design_reference_catalog_tests`。

```sh
cmake --build --preset linux-gcc-headless-debug -j2 --target <上述 targets>
cmake --build --preset linux-gcc-headless-release -j2 --target <上述 targets>
filter='^rynui\.(input_number_(model|public_api|component|theme)|color_picker_component|form_component|gallery_color_number|theme_algorithm|portable\.(theme_public_api|input_component|backend_boundaries)|select_complete_retained|select_component|auto_complete_complete_form_editor|auto_complete_flow|gallery_sample_page|ant_design_reference_catalog|gallery_sample_source_(contract|generated)|ant_design_gallery_catalog_(generator|contract))$'
ctest --preset linux-gcc-headless-debug -R "$filter" --output-on-failure
ctest --preset linux-gcc-headless-release -R "$filter" --output-on-failure
ctest --preset linux-gcc-headless-debug -R '^rynui.gallery_color_number$' --output-on-failure
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
ctest --preset linux-gcc-release -R '^rynui.gallery_native_color_number$' --output-on-failure
```

clang-format 22.1.3：904 自有文件、0 失败；catalog generator/check/self-test、80 项 source 校验、doctor healthy、strict 107/107、diff 检查通过。本阶段没有测试失败；冲突仅在生成目录和 source 检查，合并后保留两批组件检查。只新增完成 11.1/11.2、15.1/15.2，此前 19 项保持，合计 **23/135**；63.2、64.2、65.1 仍开放。

边界：上述为真实私有 Wayland/Vulkan/system-font 运行中的有限宿主合成输入，不证明物理输入、系统 IME 候选 UI、跨实际显示器/DPI、辅助技术或 Windows/MSVC。只记录这两个组件的 Linux 原生结果，没有收口整个数据录入分类。原始日志、JSON、诊断和二进制仅在忽略的 out；Release Gallery 为 `out/build/linux-gcc/examples/Release/rynui_token_gallery`。
