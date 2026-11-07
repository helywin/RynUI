# App 面板服务与异步确认

2026-11-07，Windows/MSVC `windows-msvc-headless`，Debug/Release，Ninja Multi-Config，默认栈，最多两个 compiler。107 的实现与 108 服务上下文复用同一份代码和证据。

App 新增真实 Modal/Drawer/保留锚点 Popconfirm 服务及 typed handles/futures。Modal 与 Popconfirm 共用 ConfirmAction，保留受控权威、调用处活 Theme/config、独立 entry/action 代际、有限 owner-thread 超时和退役取消。服务 Popconfirm 的占位触发器保持后代输入资格，仅自身不可聚焦；真实鼠标与键盘确认不被阻断。

## 实际验证

- 正式 Debug/Release 各构建 22 个相关 target。新增 `confirm_action`、8 个 `app_panel_*`，扩展真实 `i18n_service`，两个独立公开头 TU 随测试编译，不重复计数。
- Debug 共 74 个不同入口通过：新/扩展 20/20，旧范围 53 项首轮通过，补建清单遗漏的 notice_theme 后该项通过。Release 最终单轮 74/74，0 失败。
- 覆盖同步/worker/重复/晚到完成、Provider/Post 异常与超时、受控拒绝、真实点击、嵌套与双窗口、富输入 selection/composition/history、语言/Theme 更新、body 替换失败、重入与 owner 退役。浮层家族通过后打开的独立 Modal 遮挡/解除及实际焦点验证；退役后 nodes、输入租约/companions 和 deadline 回基线。
- 旧范围包括 Modal/Drawer/Popconfirm、共享 overlay/input constraints、Form 输入域、Notice、Spin 和后代几何。clang-format 22.1.3 检查 1138 个 tracked 源文件，0 failures；10 个新增文件另经 dry-run。doctor healthy、strict 108/108、diff check 通过。

在 MSVC Developer Environment 中使用正式 HEADLESS Debug/Release build/test presets。CTest 选择范围为以下两式的并集，原始精确 target/选择式和日志仅在 ignored `out/resume-20261107/`：

```text
^rynui\.(confirm_action|app_panel_[a-z_]+|i18n_service|app_service_[a-z_]+|app_public_api)$
^rynui\.(modal_component|popconfirm_component|portable\.(modal_boundary|modal_public_api|drawer_component|drawer_public_api|popconfirm_public_api)|window_overlay_[a-z_]+|input_constraint_[a-z_]+|form_modal_scope|form_input_domain|notice(_[a-z_]+)?|spin_completion|overlay_descendant_maintenance)$
```

本阶段是通用服务合同，不是 native GPU、系统 IME 或 Linux 结果。panel 当前即时关闭，不宣称完整退出动画；Modal/Drawer/Popconfirm 的 motion、drag/resize/blur 等剩余组件合同与平台任务保持独立。App 新 Gallery 按钮尚待 Gallery 构建和目录校验，59.2 不勾选。
