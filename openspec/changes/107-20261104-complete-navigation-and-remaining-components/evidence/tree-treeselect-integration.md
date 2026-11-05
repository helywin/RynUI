# Tree / TreeSelect 主线集成验收

2026-11-05；基线 `056350a7d6317bc816975014b302cc9be600c184`，仅按序集成本地 `14c004bc8211cdf4c0318a49e49877db19f8f9b6`、`62a90d004ec0edd0a26b20b7723e362db758eae7`。源分支保持干净、无 upstream、未修改或推送；独立实现范围见 [Tree](tree-linux.md)、[TreeSelect](tree-select-linux.md)。保留新 Form/Mentions、Transfer focus observers、主题及 SDL 有序 modifier 修复。

Linux x86_64 / GCC 16.2.1 / Ninja Multi-Config / BUNDLED，组合 HEADLESS **Debug 80/80、Release 80/80、0 失败**。沿用源分支 Tree 31、TreeSelect 25、共享 11 的逻辑基线，增加新 Form 7、Tree/Form 组合 1、AutoComplete Form 1、Mentions Form/focus 2、Transfer 1、focus lifecycle 1；未运行全量 CTest 或旧 GPU 矩阵。

```sh
cmake --build --preset linux-gcc-headless-<debug|release> --target <下列 targets> -j2
ctest --preset linux-gcc-headless-<debug|release> -j2 --output-on-failure -R '<下列正则>'
```

targets：`rynui_tree_{completion,model,component,review,r2,theme,form_integration}_tests`、`rynui_tree_select_{completion,component,model,review,r2,r3,integration,theme}_tests`、`rynui_{gallery_sample_page,token_gallery_frame,ant_design_reference_catalog,gallery_source_panel,theme_algorithm}_tests`、`rynui_form_{model,async,component,advanced,interaction,lifecycle,integration}_tests`、`rynui_auto_complete_complete_tests rynui_mentions_tests rynui_transfer_complete_tests`；portable `input_component selection_component input_public_api focus_lifecycle`。

```text
^rynui\.(tree_|gallery_sample_page$|gallery_page_tree_select_frame$|theme_algorithm$|ant_design_reference_catalog$|gallery_source_panel$|gallery_sample_source_|form_(model|async|component|advanced|interaction|lifecycle|integration)$|auto_complete_complete_form_editor$|mentions_(form|focus)$|transfer_complete$|portable\.(input_component|selection_component|input_public_api|focus_lifecycle|backend_boundaries)$)
```

额外 Core Release **8/8**：以 `linux-gcc-headless-release` 构建 `rynui_state_projection_tests` 和 portable `focus_order focus_state pointer_route interaction_registry text_input_session` targets，`-j2`；CTest 正则 `^rynui\.(state_projection_tree_(local|invalidation|retry)|portable\.(focus_order|focus_state|pointer_route|interaction_registry|text_input_session))$`。覆盖新拓扑投影、焦点、路由与会话边界。

集成回归覆盖私有 Tree 富行、TreeSelect selector/动态富标签不抢占外层 Form 自动绑定或 Enter；显式 popup FormItem 仍绑定其实际 Form；稳定 key 重排保留字段回调、删除后旧回调不写入替代字段。显式 String TreeSelectRef 全 Form 同值 reset 在 async/preedit/capture 同时存在时取消旧输入/请求，保留接受值、selector 身份与焦点。补充私有作用域并清理隐藏标签 input-block；严格资源清理断言保留。初次 Debug **79/80** 暴露旧 Gallery 只读 selector 的夹具配置回归，恢复原有配置后最终 **80/80**；初次新增测试编译变量重名已纠正。

SDL 在泵前读取一次 Shift/Ctrl/Meta，批内键盘事件按队列顺序更新鼠标 metadata；新增字段默认初始化，未逐事件读取最终全局状态。输入合同补充旧 aggregate 默认值、移动合并保留最新 metadata、失焦和 batch clear 清理。

原生 Release：`cmake --preset linux-gcc`；`cmake --build --preset linux-gcc-release --target rynui_token_gallery rynui_sdl_event_adapter_tests rynui_platform_input_tests rynui_platform_event_pump_tests rynui_platform_lifecycle_tests -j2`。`python tools/run_background_wayland.py --log-dir out/tree-integration/sdl-wayland --timeout 60 -- ctest --preset linux-gcc-release --output-on-failure -R '^rynui\.(sdl_event_adapter|platform_input|platform_event_pump|platform_lifecycle)$'`：**4/4**。

`ctest --preset linux-gcc-release -R '^rynui\.gallery_native_tree_tree_select$' --output-on-failure`：**1/1 CTest、4 页、671 checks、143 submitted frames、0 失败**。实际 Tree→TreeSelect→Form→Button，经 `run_gallery_page_acceptance.py`→`run_background_wayland.py`；Vulkan/SPIR-V/system fonts、host input area/cursor 通过。覆盖动态稳定 key/多选/strict/half、每节点合成 async、10k 虚拟行、drag/drop/DirectoryTree；TreeSelect tag/search/富 slots/async/virtual/实际回调，typed 字段实例与整表同值 reset 保留 authority/selector focus，capture/preedit/session 退休；新 Form 深列表/typed 提交/异步取消/精确金额/自动与 custom adapter/滚动聚焦后正常转入 Button。保留 frame callback、有限 pacing 和正常 SDL pump，无桌面 fallback。

五段公开 C++ 示例以 `g++ -std=c++20 -Wall -Wextra -pedantic -Iinclude -fsyntax-only out/tree-integration/docs-api.cpp` 编译通过。Catalog `--check --self-test`、source generator 生成/`--check` 和 `tests/gallery_source_tests.py` 通过，实际 **91** 个源示例；冻结 **458 IDs** inventory 与 `a0e7c7e` 字节一致。全部五份完整 **62 顶层字段** Theme golden、Theme API/runtime 与 Form 提交字节一致，无新 Theme domain。clang-format **22.1.3**、format-code `--check`：**953 文件、0 失败**；doctor healthy、strict **107/107**、`git diff --check` 通过。原始日志、JSON、帧图、编译探针和二进制只在 ignored `out/`。

节点上限 **100000 / 64 层**；富行/标签缓存随访问或接受的 key 增长。合成 async/Core composition 不证明网络服务或物理系统 IME；HEADLESS/Recording 不作为原生 GPU 证据。Windows/MSVC、系统 IME candidate UI、AT、实际跨显示器/DPI、人工桌面未验证。仅关闭 24.1/24.2、46.1/46.2，保留此前 35 项，累计 **39/135**；63.2/63.3、64.*、65.1 保持开放。Release Gallery 已重建，未启动到用户桌面。
