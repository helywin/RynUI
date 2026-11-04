# Select / AutoComplete 主分支组合验收

在 `29faf58d` 上按顺序整合本地 `c16ded39`、`a63badc2`、`a08c6b3d`、`1a2f4f58`；源 worktree 保持干净，未改写、未推送 T3 分支。来源平台通用各 43/43 的结果见两组件原记录，本阶段不重复完整集合。

- 实际环境：Linux x64 / GCC 16.2.1、Ninja Multi-Config。`linux-gcc-headless` Debug 聚焦 **25/25**；最后 Select 修复后受影响 complete 合同再跑 **5/5**。Release 聚焦 **25/25**，0 失败；另在 `linux-gcc` Release 运行 custom Tags 定向合同 **1/1**。覆盖新合同/public API、Select 单选兼容、Pagination/Tabs 组件及独立 Gallery、完整 Theme algorithm、目录/source 和 portable backend boundary。
- 实际 `linux-gcc` Release 私有后台 Wayland：AutoComplete → Select → Pagination → Tabs → Button **5/5 cases、566 checks、121 submitted frames**；Vulkan/SPIR-V/system fonts，host input-area/cursor 均为 true，无桌面 fallback。保留 106 frame callback、有限 pacing 和正常事件泵。
- AutoComplete：分组过滤/选择、窗口列表键盘显露及 wheel、真实自定义 TextArea 的多行/合成 composition/backfill/选择、保留 popup 草稿、Form reset、受控拒绝、按钮交付最新合成查询与旧查询拒绝、持 editor/session/capture 离页清理均通过。
- Select：分组 Multiple 的受控回调及实际标签关闭、UTF-8 Tags 分隔/Enter/自定义标签移除、交互 affix 和保留 popup 草稿、10,000 项 End/wheel 显露、按钮交付合成异步与旧票据拒绝、持 capture/session 离页清理通过。Pagination 的旧单选 onChange、Tabs 的 Dropdown/Menu 组合及清理同时通过。
- 具体修复：取消回调重入/销毁不得覆盖较新的异步票据；自定义 footer 不能扩大列表滚动上限；响应式富标签测量后恢复实际 selector placement，避免切换另一 Select 时使 Button 丢失已放置几何。均有定向自动合同。保留输入优先 wheel 与几何优先 hit-test，未改 renderer ABI。
- 79 个真实 source samples、458 个冻结 legacy IDs。Input.hpp 仅增加 AutoComplete 前置声明/friend；完整 Theme/golden 未改，导航 target/字段保留。普通行在 64px 合同中 realized ≤6；原生 160px 示例 ≤10。富行缓存按已访问 key 暂存非活动子树，随访问增长，删除 key 后释放，不声称全部缓存恒定空间。

构建均 `-j2`。HEADLESS 构建 target：`rynui_auto_complete_complete_tests rynui_select_complete_tests rynui_select_component_tests rynui_select_public_api_tests rynui_auto_complete_public_api_tests rynui_pagination_component_tests rynui_tabs_component_tests rynui_navigation_c_gallery_tests rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests rynui_ant_design_reference_catalog_tests rynui_theme_algorithm_tests`。对应命令：

```sh
cmake --build --preset linux-gcc-headless-debug -j2 --target <上述 targets>
cmake --build --preset linux-gcc-headless-release -j2 --target <上述 targets>
filter='^rynui\.(auto_complete_complete_(window|async|renderers|form_editor)|select_complete_(multiple|window|retained|async|lifetime)|auto_complete_public_api|select_(public_api|component)|pagination_component|tabs_component|pagination_completion_gallery|tabs_completion_gallery|gallery_sample_page|gallery_source_panel|ant_design_reference_catalog|theme_algorithm|ant_design_gallery_catalog_(generator|contract)|gallery_sample_source_(contract|generated)|portable.backend_boundaries)$'
ctest --preset linux-gcc-headless-debug -R "$filter" --output-on-failure
ctest --preset linux-gcc-headless-debug -R '^rynui.select_complete_' --output-on-failure
ctest --preset linux-gcc-headless-release -R "$filter" --output-on-failure
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
ctest --preset linux-gcc-release -R '^rynui.gallery_native_select_autocomplete$' --output-on-failure
```

早期 source 摘录未刷新曾导致 1 项生成校验失败，重新生成后通过；原生动作曾将空 commit 当作删除、把常驻壳层 Select 计入页面清理，均已修正。真实富标签几何异常经私有 Wayland GDB 调用栈定位并修复后，完整有限序列通过。原始日志、JSON、诊断和二进制均仅在忽略的 out。

clang-format 22.1.3：891 文件、0 失败；catalog generator/check/self-test、79 项 source 校验、doctor healthy、strict 107/107、diff 检查通过。任务仅 8.1/8.2、19.1/19.2 新增完成，保留此前 15 项，合计 19/135；63.2、64.2、65.1 仍开放。

边界：Gallery provider 和宿主 Pointer/Key/Text/composition 注入明确为合成，未访问外部网络。实际私有窗口/GPU/system fonts/input-area/cursor 证据不代表物理输入、系统 IME 候选 UI、跨实际显示器/DPI、辅助技术或 Windows/MSVC 实测。没有收口整个数据录入分类。Release Gallery：`out/build/linux-gcc/examples/Release/rynui_token_gallery`。
