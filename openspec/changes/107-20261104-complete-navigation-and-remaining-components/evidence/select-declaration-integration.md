# Select 声明边界修复主线集成：Linux

- 唯一来源 `6ccfd1e09c4568c32c11fe64579c70ba0124d3c8`，精确四文件增量；[来源结果](select-declaration-linux.md) 不代替下面 main 重新验证。基线为已推送 `f35edbdb1cce152123a1860de974437c3e93876f`；不含 Image/Form pending、D helpers、phase 或 Card/Statistic WIP。
- 仅延迟正在首次声明的 Select 自身边界 close intent，声明返回后下一外部 boundary 或 prepare/layout 正常协调。保留受控 authority、suppression、旧 input gates 和异常传播；不修改 ComponentHost 或吞 stale。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config；构建 `rynui_select_component_tests rynui_select_complete_tests rynui_select_declaration_tests rynui_statistic_component_tests rynui_input_component_tests`，均 `-j2`。
- 选择器 `^rynui\.(select_(component|complete_.*|declaration)|statistic_(retained|formatting|clips|lifecycle|gates|boundaries|rollback)|input_component)$`：Debug **15/15**，其中 Select **7/7**、原 Statistic **7/7**、Input **1/1**；新增 declaration **15 条断言**。原 lifecycle rich retirement 及 close 断言全部保留，0 失败/Not Run。
- Release 同一集合 **15/15**，其中 Select **7/7**、原 Statistic **7/7**、Input **1/1**；declaration **15 条断言**。两配置均 0 失败/Not Run，未修改原 Statistic/Select 断言。
- 实际 HEADLESS Core include/link 正负 guards **1/1**；clang-format 22.1.3 **1028 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。
- 任务清单仍 **68/135**；不据此关闭 Statistic、Skeleton 或平台汇总。Gallery/Theme 无改动，97 samples / 458 frozen IDs / 五份 FULL 66 域 golden 保留。
- 原始日志与本地缓存恢复记录仅 ignored out；没有新增 native/GPU 验证，不宣称 Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器通过。独立交付提交由本文件首次加入的 Git 历史及交付回执标识。
