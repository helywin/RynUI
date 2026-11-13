# Select 富前后缀 Input 认领修正

- 问题：Select 通过全局 Input 登记数量认领 selector；前后缀同步挂载另一个 Input 时，内层 Input 会先登记，导致 selector、焦点和 editor 绑定到错误实例，旁边的 Button 可保持 active 却未被布局。
- 修正：按 Select 的直接子节点身份查找已挂载的外层 Input，保留 Input 的登记时序、Button guard 和 LayoutEngine 合同。
- 新回归加入 `rynui.select_complete_retained`：prefix、suffix 分别包含真实 Button 与独立 Input，检查 selector 身份、Button placement/几何及实际点击、两个 ref 的焦点、各自编辑/search、popup roundtrip 的 draft/身份保持与清理。
- 平台通用验收实际使用 Windows / MSVC / Ninja Multi-Config，`windows-msvc-headless-debug`、`windows-msvc-headless-release`。当前整合工作树中相关九项 CTest 为 Debug **9/9**、Release **9/9**，无失败；包含 retained/async/lifetime、i18n selection、native input/observation/protocol 与 page acceptance options/runner。
- 构建 target：`rynui_select_complete_tests`、`rynui_demo_differential_native_input_tests`、`rynui_demo_differential_native_observation_tests`、`rynui_demo_differential_protocol_tests`、`rynui_i18n_components_selection_tests`、`rynui_gallery_page_acceptance_tests`。原失败 native-input 的六个源码反例全部保留，错误 popup owner 现在完成布局后仍由消费证据指出其真实归属。
- 本结果是 Core/Headless 自动验证，不代替桌面 GPU、系统 IME 或完整 Web/native paired 验收。完整 107/109 功能及其平台任务仍按各自结果追踪。
- 原始 CDB、构建、CTest 输出只保存在已忽略的 `out/`。
