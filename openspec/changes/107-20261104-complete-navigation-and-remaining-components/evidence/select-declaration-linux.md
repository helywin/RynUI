# Select 初始声明边界通知（Linux 来源记录）

来源 `6ccfd1e09c4568c32c11fe64579c70ba0124d3c8`；以下为来源结果，主线重新验证另见 select-declaration-integration.md。

- 实施 Astra `out/astra-statistic-lifecycle/report-zh.md` 的局部方向：首次声明正常建立 row/popup 门禁，但该 Select 在声明返回前不交付祖先屏蔽 close；随后外部 boundary 立即协调，没有外部 boundary 时 prepare/layout 前补交付。没有调整 ComponentHost、吞 stale 异常或删除门禁。
- GCC `linux-gcc-headless`、Ninja Multi-Config、`-j2`，Select component/complete/declaration 定向 CTest **D/R 各 7/7**；新增声明时序 **各 15 断言**。完整 Statistic retained/formatting/clips/lifecycle/gates/boundaries/rollback **各 7/7**，原 lifecycle 的 16 个 rich retirement case 和原 close 计数不变，后续 blocked ancestor、initial retirement、deferred loading/blur/formatter 自毁均执行。
- 命令：构建 `rynui_select_component_tests rynui_select_complete_tests rynui_select_declaration_tests rynui_statistic_component_tests`；`ctest --test-dir out/build/linux-gcc-headless -C <Debug|Release> -V -R '^rynui.select_(component|complete_.*|declaration)$'` 与 `'^rynui.statistic_(retained|formatting|clips|lifecycle|gates|boundaries|rollback)$'`。
- 新断言保留 controlled authority/suppression：声明内无提前 close、下一外部 boundary 关闭一次；恢复祖先不自动 reopen，显式 false→true 可重开；dispose 无资源残留。两条私有 Wayland 原生序列另行通过，不用它们代替这些 CPU case。
- 初次失败的16例根因有当前 GDB 证据；历史 `13b125ae` 归属仅为源码推断，未运行历史 baseline。此次提交不关闭 Statistic 40.* 或 Skeleton 55.*。clang-format 22.1.3/diff check 通过；Windows、物理 IME/AT 未验证；原始材料只 out。
