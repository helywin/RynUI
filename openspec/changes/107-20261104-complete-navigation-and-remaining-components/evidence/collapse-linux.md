# Collapse · change 107 Linux 验证

源基线为隔离 checkout `d3431206`（Theme 62 字段、91 samples）；不覆盖父线程随后合入 main 的 64 字段/93 samples 或 Image/Upload RGBA。设计基线：Ant Design 6.6.5，commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；官方组件 API/types/style/demo/test 源码仅置于忽略的 `out/upstream/`。保留旧 vector 调用及已有示例，typed 原生映射见 `docs/collapse.md`。

实现与定向回归：稳定 key 的增删/重排与局部 title/body/extra revision，保留实际 Input editor/ref/selection；受控拒绝及回调重入；显式 lazy、forceRender 优先级和动画结束后 destroy；真实裁剪高度/旋转箭头中点、反向、取消和 idle；ghost、RTL、custom icon 与 Header/Icon/Disabled 门控。lazy/局部替换继承语义字体与前景色，custom icon 销毁 owner 后立即退出刷新。

四组件均使用真实 Form/FormItem 自动绑定 Input、同值 reset、Enter submit、Select 和含 Select 的 Popover，验证内层 Escape、Tab/ShiftTab 与销毁清理；未隔离用户显式 FormItem。Core 只更新 logical CPU scene，未增加 renderer/SDL/平台依赖。

平台通用检查实际运行于 Linux/GNU、Ninja Multi-Config：`linux-gcc-headless-debug` / `linux-gcc-headless-release`。两配置分别构建 30 个受影响 target（`-j2`），定向 CTest Debug **49/49**；Release 首轮 **48/49**，唯一失败为构建启动后补入合成数据标签造成的 source metadata 缓存过期。重新生成后仅复测失败项 **1/1**，全部 49 个不同检查均已通过；其中本组件每配置核心 **4/4**、Gallery **1/1**。最终阶段独立 CTest 的核心复测亦每配置 37/37，失败为 0。完整 49 项包含四组件核心 37、四页 Gallery 4、Theme/Core boundary/目录/源码/示例 8；没有运行全量 CTest。最终四条 overlay promotion 后，Gallery/目录/源码定向复核每配置 **9/9、0 失败**。

```text
ctest --preset linux-gcc-headless-debug -R '^rynui.(collapse_|segmented_|descriptions_|timeline_|theme_algorithm$|portable.backend_boundaries$|ant_design_reference_catalog$|gallery_sample_page$|gallery_source_panel$|gallery_sample_source_contract$|gallery_sample_source_generated$|data_display_docs_examples$)' --output-on-failure
ctest --preset linux-gcc-headless-release -R '^rynui.(collapse_|segmented_|descriptions_|timeline_|theme_algorithm$|portable.backend_boundaries$|ant_design_reference_catalog$|gallery_sample_page$|gallery_source_panel$|gallery_sample_source_contract$|gallery_sample_source_generated$|data_display_docs_examples$)' --output-on-failure
```

本组件 target 的精确复现命令如下；实际联合构建的完整 30-target 列表及日志仅留 `out/affected-targets.txt`、`out/logs/current-*-build.log`。

```text
cmake --build --preset linux-gcc-headless-debug --target rynui_collapse_public_api_tests rynui_collapse_theme_tests rynui_collapse_component_tests rynui_collapse_contract_tests rynui_data_display_gallery_tests -j2
cmake --build --preset linux-gcc-headless-release --target rynui_collapse_public_api_tests rynui_collapse_theme_tests rynui_collapse_component_tests rynui_collapse_contract_tests rynui_data_display_gallery_tests -j2
```

失败项复测命令：`ctest --preset linux-gcc-headless-release -R '^rynui.gallery_sample_source_generated$' --output-on-failure`。独立组件阶段随后按组件/配置分别复测，核心共 37/37 每配置通过，详见各阶段本地提交。

原生检查：`cmake --build --preset linux-gcc-debug --target rynui_token_gallery -j2`，再运行：

```text
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Debug/rynui_token_gallery --log-dir out/native-final --preset linux-gcc --configuration Debug --timeout 120 --page collapse --page segmented --page descriptions --page timeline --page button
```

仅经既有 runner → `tools/run_background_wayland.py`，独立 headless Wayland/private DBus。实际 Vulkan/SPIR-V、系统字体、host input area/cursor 同步：**5/5 case、225 checks、45 submitted frames，0 失败**。动作验证真实草稿、切换/有限 motion、重排/布局/键盘/回调和跨页 capture/session 退役；Timeline loading 后明确关闭。未修改 106 frame callback、动作 pacing 或 SDL pump。早期尝试在目标滚动 reveal 前切 Theme motion，第二帧 callback 超时；调整为 reveal 后启用示例 motion 后，上述完整流程通过。失败日志只留 `out/`。

共同检查：91 个 samples 与 458 个 legacy IDs 保持；源码 generator、catalog `--check --self-test` 通过；10 个公开旧/新文档示例使用实际 Signal API 编译。5 份 FULL Theme goldens 经 writer 重生成，与基线逐字节一致，各 62 个顶层字段，无无关 domain/TokenIdentity 变动。clang-format 22.1.3 检查 964 个自有源文件，0 失败；OpenSpec doctor healthy，strict 107/107；`git diff --check` 通过。

边界：Linux HEADLESS 与私有 Wayland 是 Core/原生后端组合证据；不宣称 Windows/MSVC、物理系统 IME、AT/screen reader、实际桌面或跨物理显示器实测。Web DOM/CSS/React/HTML 行为按公开文档映射原生合同，不作为未实现的原生功能。原始 JSON/log/二进制/源码副本仅在忽略的 `out/`，本证据不包含这些产物。
