# Transfer Linux 隔离阶段验收

主分支完整集成与组合验收见 [transfer-integration.md](transfer-integration.md)；下列 59 个既有 Theme 字段和 82 个源示例属于隔离起点，不能当作当前 main 数量。

23.1/23.2 已完成。此结果来自隔离起点 `6212a9c5` 及导入的组件阶段；不覆盖父分支随后完成的 Cascader/DateTime 内容。

- 固定参照 Ant Design 6.6.5、commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 API/types、实现与全部公开 demos。25 份源码及哈希仅保存在忽略的 `out/transfer/upstream/`。
- 平台：Linux GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config、BUNDLED、`-j2`。通过 preset 分别构建 Transfer/Theme/public API、Input/TextArea/selection/focus、Pagination 和 Gallery/catalog/source 相关 targets。
- `ctest --preset linux-gcc-headless-{debug,release} -R '^(rynui\.(transfer_theme|theme_algorithm|theme_runtime|pagination_component|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog|gallery_pages_host|gallery_color_number)|rynui\.portable\.(transfer_component|transfer_complete|theme_public_api|input_component|text_area|selection_component|focus_lifecycle|backend_boundaries))$' --output-on-failure`：最终 Debug **17/17**，Release **17/17**，最终失败 0；未运行全量 CTest。
- Transfer 基线 6 场景及新增 15 合同组通过：实际 scoped Token/paint/layout、动态几何与六类非法组合、locale/RTL/counts/actions、保留 custom list/footer、受控与批处理请求、14 类失败边界、9 类自销毁/失败清理、8 类 rich Input gate、composition/capture/focus、Tab/Escape/scroll、范围选择、真实 RTL Pagination/Select 页大小拒绝/接受、稳定重排/可见性及初始 factory context。Gallery 已通过真实自定义 Table 选择和 action 移动回调。
- Theme **36** 个字段的 hash/独立 dirty domain、seed/algorithm、scope/inherit/reset 与非法 metrics 通过；`python tests/verify_transfer_theme_baseline.py` 验证五份完整 golden，保留既有 59 个顶层字段（identity 因新增 Token 改变）。Transfer 为 17 色、13 metrics、typography/effects/order，实际控件与 logical scene 消费这些值。
- 验证中修复了分页 size changer 命中区域、自定义列表初始挂载、隐藏全选时的计数、操作文字自然宽度、批处理权威键校验与 getter 自销毁；最终定向测试均通过。一次新增测试的未显式 typed array 初始化导致编译失败，已修正后重新构建通过。
- 原生 Release：`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2` 成功。`python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page transfer --page button --timeout 120 --log-dir out/transfer/native` 通过；仅由 runner 调用 `tools/run_background_wayland.py` 的私有 compositor，没有桌面 fallback。Vulkan / SPIR-V / system fonts，**2 cases / 125 checks / 23 submitted frames**，host input area/cursor sync 均通过。
- 原生序列实际验证受控行移动与回调、本地化/RTL/动态尺寸、fractional scroll、Token header GPU pixel、富行 Input 草稿移动，以及 composition/capture/session 离页清理。它是有限程序化输入/GPU 证据，不代表物理键盘、系统 IME 候选窗或 OS 辅助技术。
- `update_ant_design_gallery_catalog.py --check --self-test` 和 `generate_gallery_source.py --output out/transfer/gallery-source.hpp --check` 通过。本隔离分支实算 **82** 源示例，Transfer 页 **3** 个实际实例；**458** frozen legacy IDs 保留。host inventory 对照实际 generated source registry，不再固定旧的 80；集成时按父分支的完整目录重新生成。
- clang-format **22.1.3**：917 个 owned source，0 failures；OpenSpec doctor healthy、strict **107/107** 与 `git diff --check` 通过。原始 log/JSON/图片/二进制/源码副本仅位于忽略的 `out/transfer/`。

HEADLESS 与以上私有 Wayland 序列不证明 Windows MSVC/D3D12、物理系统 IME 候选窗或 OS 辅助技术。63.2/64.2/65.1 由父线程管理；Cascader 在此分支保留导入状态，不修改其实现和剩余任务。
