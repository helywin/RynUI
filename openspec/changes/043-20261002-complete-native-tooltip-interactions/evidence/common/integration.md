# 原生功能整合

2026-10-02，Windows/MSVC，Ninja Multi-Config。

- `windows-msvc-headless-debug` / `windows-msvc-headless-release` 完整 build 与 CTest 两配置均 40/40（Release 13.69 秒）；包含 Core/renderer/backend 边界守卫、pointer route、既有组件、logical scene、atlas 和 allocation 合同。
- Gallery 新增 retained 富标题 click、ContextMenu、hover/focus/click 组合与居中角箭头三样例；live_samples=70，Button offset=24、注册 interaction offset=124、可见 interaction offset=103。Windows native preset 的共用 Gallery frame 合同 1/1 通过（16.12 秒）。
- Tooltip 支持目录改为原生 implemented，仅列 Web DOM/CSS/portal 排除项；组件收尾清单仍分开记录平台验收，不把实现状态代替 Linux/Windows 验收。
- 目录重新生成、图标资产/许可验证通过；clang-format 22.1.3 检查 395 个文件、0 failures；doctor healthy、full strict 43/43、git diff --check 通过。

Windows 多 DPI 的 rich/action/vector-arrow 矩阵属于 task 5.1；Linux native 独立待验。实现阶段没有 push、PR 或 archive。
