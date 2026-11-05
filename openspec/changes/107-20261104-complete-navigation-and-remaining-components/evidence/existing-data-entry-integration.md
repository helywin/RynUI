# 现有 DataEntry 控件主线集成验收

2026-11-05；基线 `29089c3cdf5c89d45ccf5fabeb025624d3d73331`，仅按序集成本地 `fad7b9738f9661a916d0970e82a8508119528b77`、`31dcdc12bdb78db2fb0a50b0cb5ca54a50469437`。源分支干净、无 upstream、未修改或推送；实现范围和独立 36 项逻辑基线见 [existing-data-entry-linux.md](existing-data-entry-linux.md)。InputHost 实现沿用，保留新 Form ref/geometry/generation/自动绑定、Mentions/Transfer focus observers、Tree 输入门禁及有序 SDL Shift/Ctrl/Meta metadata。

新增 Form 组合回归以实际 Checkbox/Switch 自动绑定验证 bool 回写、真实 control geometry、reset/disabled 门禁、focus/blur callback 自销毁与异常；旧字段回调和 ref/control target 在整表退休后失效，捕获/焦点/场景无残留。十个组合场景覆盖两类控件的正常、focus 销毁、blur 销毁、blur 抛错、focus 抛错；首次测试编译缺内部 Form 宿主头文件，补齐后定向 **2/2** 通过。原有 compound private editor、富 popup、Transfer 局部退休断言保留。

Linux x86_64 / GCC 16.2.1 / Ninja Multi-Config / BUNDLED。实际命令：`cmake --preset linux-gcc-headless`；`cmake --build --preset linux-gcc-headless-<debug|release> --target <下列 targets> -j2`；`ctest --preset linux-gcc-headless-<debug|release> -j2 --output-on-failure -R '<下列正则>'`。复用源分支未改 Input 家族逻辑基线，仅运行受本次合并影响的 completion/Rate、Form、selection/Slider/focus、Gallery/Theme/公开边界集合。

targets：`rynui_existing_data_entry_completion_tests`、`rynui_rate_{component,lifecycle,public_api,theme,focus_order}_tests`、`rynui_form_{model,async,component,advanced,interaction,lifecycle,integration}_tests`、`rynui_tree_form_integration_tests rynui_mentions_tests rynui_auto_complete_complete_tests`、`rynui_{gallery_sample_page,token_gallery_frame,gallery_source_panel}_tests`；portable `selection_component switch_features checkbox_group radio_features slider_component selection_controls_public_api switch_public_api checkbox_public_api radio_public_api focus_order focus_state focus_lifecycle theme_algorithm theme_runtime`。

```text
^rynui\.(existing_data_entry_completion|rate_(component|lifecycle|public_api|theme|focus_order)|form_(model|async|component|advanced|interaction|lifecycle|integration)|tree_form_integration|mentions_form|auto_complete_complete_form_editor|gallery_(page_input_frame|page_members-metrics_frame|sample_page|source_panel|sample_source_contract|sample_source_generated)|ant_design_gallery_catalog_(generator|contract)|portable\.(selection_component|switch_features|checkbox_group|radio_features|slider_component|selection_controls_public_api|switch_public_api|checkbox_public_api|radio_public_api|focus_order|focus_state|focus_lifecycle|theme_algorithm|theme_runtime|backend_boundaries))$
```

组合结果 **Debug 39/39、Release 39/39、0 失败**。补充 Slider 捕获中 blur 抛错的退休断言，Release 已包含此断言；最终再以 Debug 构建 `rynui_existing_data_entry_completion_tests -j2`，CTest 正则 `^rynui\.existing_data_entry_completion$` **1/1** 通过。生产清理逻辑无需额外修复，未放宽既有断言。Rate 的真实 glyph/RTL 半星裁剪、pointer/keyboard、逐项 Prop 身份/增长回滚、ref 线程/重绑、事件异常与退休，以及新 Form 同步/异步/布局/List/ownership 合同通过。未运行全量 CTest 或旧主题×尺寸 GPU 矩阵。

原生：`cmake --preset linux-gcc`；`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2`；实际有限序列：

```sh
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --log-dir out/existing-data-entry-integration/native --preset linux-gcc --configuration Release --timeout 120 --page checkbox --page radio --page switch --page slider --page rate --page input/input --page input/search --page input/password --page input/text-area --page input/otp --page button
```

**11/11 页、585 checks、110 submitted frames、0 失败**，runner return code 0、desktop_fallback=false。仅经 `run_gallery_page_acceptance.py`→`run_background_wayland.py` 私有 headless Wayland；Vulkan/SPIR-V/system fonts、host input area/cursor sync 通过。实际受控选择/focus/动态项、Slider range 拖动与 MultiSlider 门禁、Rate 半星/拒绝/RTL/动态字符/ref/富 Tooltip 草稿保留、五个 canonical Input family 的 Unicode/提交/formatter/mask/多行与 session/preedit/capture 离页清理通过。保留 frame callback、有限 pacing、正常 SDL pump。新 Form 自动绑定/异常组合由上述 HEADLESS 实测，沿用前三阶段原生 Form/Tree 结果，未重复旧整批窗口测试。

Catalog write/check/self-test、source generator 生成/`--check` 和 `tests/gallery_source_tests.py` 通过，实际 **91 sources**；Form/List 与其他 extractors 保留。冻结 **458 IDs** inventory 与 `a0e7c7e` 字节一致。全部五份完整 **62 顶层字段** Theme golden、Theme API/runtime 与 `29089c3c` 字节一致。clang-format **22.1.3**、format-code `--check`：**957 文件、0 失败**；doctor healthy、strict **107/107**、`git diff --check` 通过。日志、JSON、逐帧图、编译探针及二进制仅在 ignored `out/`。

RateCharacter 继续是 String/IconSource 两层裁剪的纯绘制值；富 Tooltip 单次挂载，不承诺克隆两个有状态 Component/ref/session。HEADLESS/Recording 不作为原生 GPU 证明，合成 composition 不证明物理系统 IME。Windows/MSVC、系统 candidate UI、AT、实际跨显示器/DPI 和人工桌面未验证。只关闭 10.*、14.*、17.*、18.*、20.*、21.* 共 **12 项**；此前 39 项保留，累计 **51/135**。63.2/63.3、64.*、65.1 仍开放。Release Gallery 已重建，未启动到用户桌面。
