# Tabs Linux 平台通用验收

- 基线：`f20a77415d8f37c185f0de74dfe5f570cbbed06a`；上游 Ant Design 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd`。源码/API/demo 下载仅在忽略的 `out/upstream/`。
- 实际环境：Linux x64，GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config；构建始终 `-j2`。
- 实现和覆盖：动态 key 编辑与邻项回退、lazy/forceRender/destroyOnHidden、富 label/extra、四方向/RTL、原生 Dropdown/Menu 溢出、自定义 indicator、有限动画/reduced motion；保留草稿、输入捕获/焦点/IME/popup 清理与工厂异常回滚。
- 构建：`cmake --build --preset linux-gcc-headless-<debug|release> --target rynui_tabs_model_tests rynui_tabs_theme_tests rynui_tabs_public_api_tests rynui_tabs_component_tests rynui_token_gallery_frame_tests -j2`。
- CTest：`ctest --preset linux-gcc-headless-<debug|release> -R '^rynui\.tabs_(model|theme|public_api|component)$' --output-on-failure`；Debug **4/4**、Release **4/4**。
- 实际 Gallery 定义及 Recording frame executable 已构建；`out/build/linux-gcc-headless/tests/<Debug|Release>/rynui_token_gallery_frame_tests --select-card-tabs-only` 在 Debug/Release 各 **1/1** 通过。保留 frozen legacy IDs。
- `format-code.py --check`（clang-format 22.1.3）通过；目录生成/自测、OpenSpec doctor、strict **107/107**、`git diff --check` 通过。
- 边界：这是平台通用逻辑/Core 输入分发的合成事件/Recording scene 证据；未执行原生窗口/GPU/系统 IME、辅助技术或实际 Windows MSVC。本子分支不勾选 63.1、64.*、65.1，不 push、不建 PR、不 archive。

三页定向集成：`rynui_navigation_c_gallery_tests pagination|steps|tabs` 在 Debug/Release 各 **3/3** 通过，验证真实独立页扩展示例、重复响应布局稳定与销毁清理；加上组件合同，每配置 **10/10**；另有 Steps 既有焦点/生命周期回归 **7/7**，最终合计 **17/17**。

保留策略明确为 forceRender 显式优先；新增隐藏强制面板在普通属性更新和切换中不重挂载、被裁剪 header 子控件聚焦自动显露的回归，均包含于最终 Debug/Release 17/17。

目录提升后，历史 Tabs partial/dynamic 缺口断言同步为已实现及平台边界合同。构建 `rynui_ant_design_reference_catalog_tests`、`rynui_navigation_c_gallery_tests`、`rynui_token_gallery_frame_tests`；Debug 的目录与三页定向集成 **4/4** 通过。
