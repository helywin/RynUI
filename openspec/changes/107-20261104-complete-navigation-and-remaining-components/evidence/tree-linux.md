# Tree / DirectoryTree Linux 验收

46.1/46.2 完成；Tree 依赖先于 TreeSelect 实施。固定参照 [Ant Design 6.6.5 Tree/types](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/tree/Tree.tsx)、DirectoryTree、dynamic/draggable demos 和 directory tests；源码仅缓存于忽略的 out。

- 稳定 key 全量拓扑验证后发布；新增/移除/重排/跨父移动保留存活 editor；contentRevision 控制富工厂替换。多选、Ctrl/Meta/Shift 指针及键盘范围、strict/级联/half 权威、禁用隔离、过滤/线段/向量图标/RTL/DirectoryTree、有限与减弱 motion 已完成。
- 每节点独立可取消 loader，owner-thread post、generation/重复/过期拒绝、错误/重试/loadedKeys、乱序返回、reset/移除/卸载、provider throw/自销毁/错误 dispatcher 和非法数据回滚有定向自动覆盖；Core 不访问网络。
- **10000** 普通虚拟行实测最多 **16** realized rows；**10001** 节点级联勾选/取消通过。连续滚轮不增长普通行实例；可变行高和 refs/scrollTo、富行输入封锁复用既有输入合同。真实 capture 拖放支持 before/inside/after、循环/disabled 拒绝及边缘持续滚动；源行离窗后由可见根保留 capture，drop/cancel/隐藏/拓扑变化清理。
- Tree targets：`rynui_tree_completion_tests rynui_tree_model_tests rynui_tree_component_tests rynui_tree_review_tests rynui_tree_r2_tests rynui_tree_theme_tests`；构建命令：`cmake --build --preset linux-gcc-headless-debug --target rynui_tree_completion_tests rynui_tree_model_tests rynui_tree_component_tests rynui_tree_review_tests rynui_tree_r2_tests rynui_tree_theme_tests -j2`；Release 将 preset 替换为 `linux-gcc-headless-release`，其余参数完全相同。原有回归保留；zero_area 的旧断言在基线 host 亦失败，修正为仅排除关闭的嵌套 Select 自有 window-layer gates，并断言该 Select 仍关闭；Tree 身份/草稿/IME/capture/foreign block 与销毁后的精确 gate 检查保留。
- 原生 Tree 页验证实际选择/勾选回调、稳定重排、捕获拖放应用回写、10000 行 scrollTo、两个逆序交付的明确标注合成异步源和离页清理。目录页保留原项目/富备注样例，并增加 **4** 个 Tree/DirectoryTree 实例。
- 实施中修复 defaultExpandAll 挂载期虚拟行工厂上下文、批量勾选与范围成员集合、虚拟拖放焦点回滚和 Meta+Shift 增量范围；最终定向结果全部通过。明确限制：**100000** 节点、**64** 层；富标题/自定义图标缓存按已访问 key 增长，普通虚拟行有界；应用负责实际重排和异步 transport，浏览器/OS 外部 DND 不提供。

- Linux GCC **16.2.1**，Ninja Multi-Config、BUNDLED，编译并行度 `-j2`；`linux-gcc-headless` 的 Debug/Release 受影响集合最终各 **67/67**、失败 **0**：Tree **31**、TreeSelect **25**、共享 Theme/Gallery/source/Input/selection/Core boundary **11**。目录元数据更新后，共享受影响集合 Debug/Release 再各 **11/11**。未运行全量 CTest 或 GPU 矩阵。
- 共享 build targets 为 `rynui_gallery_sample_page_tests rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests rynui_gallery_source_panel_tests rynui_theme_algorithm_tests rynui_portable_input_component rynui_portable_selection_component rynui_portable_input_public_api`；均由两个对应 build presets 构建。实际 CTest 命令（两种配置分别执行）：

```sh
ctest --preset linux-gcc-headless-debug --output-on-failure -R '^rynui\.(tree_|gallery_sample_page$|gallery_page_tree_select_frame$|theme_algorithm$|ant_design_reference_catalog$|gallery_source_panel$|gallery_sample_source_|portable\.(input_component|selection_component|input_public_api|backend_boundaries)$)'
ctest --preset linux-gcc-headless-release --output-on-failure -R '^rynui\.(tree_|gallery_sample_page$|gallery_page_tree_select_frame$|theme_algorithm$|ant_design_reference_catalog$|gallery_source_panel$|gallery_sample_source_|portable\.(input_component|selection_component|input_public_api|backend_boundaries)$)'
```

- 原生 Release 构建：`cmake --build --preset linux-gcc-release --target rynui_token_gallery rynui_sdl_event_adapter_tests rynui_platform_input_tests -j2`。`python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page tree --page tree-select --page button --timeout 120 --log-dir out/tree/native-final`：**3 cases / 229 checks / 47 submitted frames**，Vulkan/SPIR-V/system fonts，host input area/cursor sync 通过。runner 仅启动私有 `run_background_wayland.py`，无桌面 fallback。
- `python tools/run_background_wayland.py --log-dir out/tree/native-input/wayland --timeout 45 -- ctest --preset linux-gcc-release --output-on-failure -R '^rynui\.(sdl_event_adapter|platform_input)$'`：**2/2**，包含批内有序 Shift/Ctrl/Meta 更新，未逐事件读取最终 SDL modifier state。
- `python tools/generate_gallery_source.py --output out/tree/generated_gallery_sample_source.inc` 后 `--check`、`python tests/gallery_source_tests.py`、`python tools/update_ant_design_gallery_catalog.py --write` 后 `--check --self-test` 通过。仍为 **87** 个源示例、**458** 个冻结 legacy IDs；Tree/TreeSelect 分别有独立页面和真实回调。5 段公开文档 C++ 示例以 `g++ -std=c++20 -Wall -Wextra -pedantic -Iinclude -fsyntax-only out/tree/docs_api.cpp` 编译通过。
- `rynui_theme_algorithm_tests --write-goldens` 重新生成全部五份完整 golden，均为 **61** 个顶层字段，内容无差异；保留 Tree/TreeSelect/ColorPicker/Cascader/Transfer。复用既有独立组件 Token，无新 Theme domain。
- clang-format **22.1.3**，`python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：**929** owned sources、0 failures；OpenSpec doctor healthy、strict **107/107** 和 `git diff --check` 通过。初次 TLS 字体下载失败后复用主仓库 `out/` 中锁定 BUNDLED 缓存，未更改依赖锁。

这些是 HEADLESS logical scene/Core 输入与有限私有 Wayland 程序化交互证据；不替代 Windows/MSVC/D3D12、物理系统 IME 候选窗、跨实际显示器或 OS 辅助技术。63.2/63.3、64.*、65.1 由父线程管理，未勾选。原始 log/JSON/图片/二进制、上游源码副本与编译探针仅在忽略的 `out/tree/`；不推送本分支。
