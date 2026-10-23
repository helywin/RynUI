# 平台通用整体验证记录

运行于本 change 的隔离 worktree，Windows / MSVC 19.51 / Ninja Multi-Config；独立 `out/build/windows-msvc-headless`，build 与 CTest 最大 4 jobs。未复用其他工作树的 binary tree。

本轮完整 CTest：

| 配置 | 结果 | CTest 耗时 |
| --- | --- | --- |
| Debug | 119/119 | 324.65 s |
| Release | 119/119 | 14.73 s |

Debug 的既有 `input_scene_allocation` 使用默认 256 Input、两种模式各 20,000 次更新，单项 293.03 s 通过，未缩减样本或跳过。完整回归之后只修正 Table 的不可见内容 autofocus、结构更新依赖隔离和外部固定高度下的 scrollY 上限；最终源码在两个配置均复测 8/8，包括 Table/Pagination、Prop/响应生命周期、主题 golden 和 HEADLESS 架构边界，具体矩阵见 `table.md`。没有重复无关的完整输入分配 benchmark。

正式命令（Visual Studio Developer Environment）：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4
ctest --preset windows-msvc-headless-debug --parallel 4
cmake --build --preset windows-msvc-headless-release --parallel 4
ctest --preset windows-msvc-headless-release --parallel 4
```

本机 helper 仅用于载入 MSVC 环境并执行同一 preset，位于忽略的 out 目录，未提交机器路径或工具。

- clang-format 22.1.3；`format-code.py --check` 检查 508 个自有文件，0 failures。Pagination 测试补充段的格式也在验证提交中收口。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 62 passed / 0 failed。
- `git diff --check` 通过；五组 golden 解析对比保留全部既有组件 token 值。
- CPU scene v3 / GPU ABI v2 未修改；实际 HEADLESS include/link 守卫通过。

根据用户更新后的协作策略，后续仅创建本地 commit，不再 fetch/rebase/push 工作树分支；父任务负责合并和 main 推送。早期规划与 Pagination 推送发生在该策略更新前，远端 t3 分支已由父任务删除；本地保留全部实现。没有创建 PR 或 archive。

本记录只覆盖平台通用合同；原生 GPU/Linux、DPI/真实窗口人工视觉、系统 IME 及全局 Gallery/catalog/support overlay/README 整合由父任务另行处理。
