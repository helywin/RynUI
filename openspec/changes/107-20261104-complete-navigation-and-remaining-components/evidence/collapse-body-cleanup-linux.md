# Collapse body 输入屏蔽释放：Linux 平台通用结果

以下为来源提交 `dee82175bb49e877c879d09c1380416dfcec8bd8` 的独立工作树记录；当前 main 重新验证见 [局部 cleanup 集成](collapse-body-cleanup-integration.md)。来源含未交接 Listy 的补充结果不计入 main。

- 问题：Gallery 页面退役后，Collapse 的关闭 body 已释放，其完整 ComponentId 仍残留在 InteractionRegistry 的 blocked roots。Astra 只读逐 ID 轨迹定位到 body 缺少 owned unblock；固定 `4d722fbc` 源码也有该遗漏，未运行该历史 executable，不将源码比较称为历史实测。
- 修复：body 建立后、用户 factory 前注册独立 resource cleanup，按完整 ticket 直接调用 `interactions().set_input_blocked(body, false)`。不经服务层 boundary callback，不清空或 prune Registry，不修改 translation phase、lease 或 Listy。
- 独立交付仅含 Collapse 三行修复、新回归源码、八行 CMake 注册和本说明。回归不调用 Listy、Gallery 或新 phase API；可在已有 Collapse/WindowComponentServices 基线上构建。当前实际构建还包含工作区 Listy WIP，Gallery 验证仅作为补充，不是此修复唯一证据。
- Linux x86_64 / GCC / `linux-gcc-headless` / Ninja Multi-Config，使用 `cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_collapse_block_cleanup_tests rynui_collapse_component_tests rynui_collapse_contract_tests rynui_component_failure_retirement_tests rynui_modal_component_tests rynui_input_component_tests rynui_select_component_tests rynui_popover_component_tests rynui_translation_ownership_tests rynui_retained_translation_phase_tests -j2`。独立 `rynui.collapse_block_cleanup` **D/R 各 1/1**，覆盖 8 个场景：partial panel 退役保留活祖先/兄弟屏蔽、destroyOnHidden 仅退役内容、初始/增量 factory 原异常与自毁、cleanup 创建新 owner、强制同 slot 不同 generation/旧 ticket、双窗口隔离、quiet retirement/待处理 focus、重复 dispose 与 idle。
- 定向 CTest `^rynui.(collapse_block_cleanup|collapse_component|collapse_contract|component_failure_retirement|modal_component|input_component|select_component|input_component|popover_(lifecycle|boundaries)|translation_ownership|retained_translation_phase)$` **Debug 11/11、Release 11/11，0 失败**。
- 额外构建 `rynui_listy_gallery_tests -j2`，其 baseline 参数与无参数两个进程分别执行 overview→Button 与富 Listy 编辑/草稿恢复、List→Listy→Button；**D/R 各 2/2**，原 nodes/scenes/glyphs/surfaces/hits/blocks/deadline 全零断言通过。此补充依赖尚未提交的 Listy/Gallery feature，不随共享修复导入。
- clang-format 22.1.3 检查 **1026 文件/0 失败**；OpenSpec doctor healthy、strict **107/107**、staged/working diff whitespace 均通过。无新增功能 checkbox。
- HEADLESS 是 CPU 合同，无 GPU、Windows/MSVC/D3D12、物理 IME、AT 或实际跨显示器结论。原始日志/诊断轨迹仅保存在忽略的 out。35/36 功能 checkbox 不由此阶段完成。
