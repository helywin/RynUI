# Popconfirm 平台通用 Debug 证据

日期：2026-10-23。隔离工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-898695bd`，分支 `t3/native-popconfirm`，父提交 `4e258e2afe832323c934e4ebfc3fb2144fc1608f`。未改 root checkout。

## 环境与命令

- Windows/MSVC 19.51.36256.0，`windows-msvc-headless`，Ninja Multi-Config，Debug，`--parallel 4`。构建目录为本工作树 `out/build/windows-msvc-headless`。
- `cmake --preset windows-msvc-headless` 后，构建以下 `rynui_portable_<name>` targets。
- `ctest --preset windows-msvc-headless-debug --parallel 4 -R 'popconfirm|modal_|select_component|tooltip_component|window_overlay|focus_order|focus_state|focus_lifecycle|theme_runtime|theme_algorithm|backend_boundaries'`：18/18通过，最终一轮2.74秒。
- 初次 Theme golden 检查因新增 Popconfirm diagnostic 段失败；使用既有 `rynui_portable_theme_algorithm --write-goldens` 更新五份 golden。逐个 JSON 比较父提交，移除新增 `popconfirm` 与 `identity` 后完全相同，其他 Token 无变化。

## 通过的合同

`popconfirm_model`、`popconfirm_public_api`、`popconfirm_theme`、`popconfirm_component`：4/4。

- 默认/受控显示、请求去重；忽略受控关闭请求后切换 Modal，仍强制隐藏且不复现。
- 真实 small primary Button，确认自动关闭、loading 保留/防重复、取消、showCancel、disabled、动态文字和 slots 身份保留。
- Enter/Space、Escape、外部点击、有效触发焦点恢复和外部入口焦点保留；初始隐藏 Input 不启动会话。
- Modal 内 Popconfirm/Select 子菜单，点击菜单行实际选值，Escape 子层优先，关闭父层清理子菜单。
- 12方位和窗口夹紧、arrow=false；Theme 纯颜色不执行 slots 或 layout。
- owner 销毁、确认回调销毁、打开回调销毁及非法声明原子拒绝。
- Theme 继承/reset、独立 seed/algorithm、五组失效、非法尺寸/字重/z-index 原子拒绝。

相关回归14/14：`modal_model`、`modal_public_api`、`modal_theme`、`modal_boundary`、`modal_component`、`select_component`、`tooltip_component`、`theme_algorithm`、`theme_runtime`、`focus_order`、`focus_state`、`focus_lifecycle`、`window_overlay`、`backend_boundaries`。

## 最小共享修改

- 新增 Popconfirm theme 字段、解析/identity/diagnostic/订阅和 golden；共享 CMake 与 window service 只作注册。
- `ComponentHost::paint_traversal` 令交互子 layer 的有效优先级高于非 Modal 父 layer；必要原因是 Select1050位于 Popconfirm1060内时，原排序既遮住菜单也违反 HitTest 的 parent-before-child 合同。保持 Modal 分组与无关层 Token 顺序，使用 int64 防止优先级极值溢出。
- 不包含父任务正在修复的 Tooltip 边界请求去重或 Slider ancestor clipping。

## 范围边界

此证据为平台通用窗口服务 fixture，使用锁定验证字体；没有启动原生窗口/GPU，也没有人工系统 IME 验收。Windows 原生/DPI、Linux 原生保持独立 pending；Release 未在此子任务执行。Gallery、support overlay、README 和父任务完成状态未修改。未改变 logical CPU scene v3、GPU ABI v2 或 SDL 公开边界。
